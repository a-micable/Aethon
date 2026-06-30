#include "aethon/runtime/batching_policy.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::runtime {
namespace {

std::uint8_t rank(IngestPriority priority) {
    return static_cast<std::uint8_t>(priority);
}

bool is_control_like(const protocol::Packet& packet) {
    return packet.kind == protocol::PacketKind::control
        || packet.kind == protocol::PacketKind::capabilities
        || packet.kind == protocol::PacketKind::heartbeat;
}

void update_window(PacketBatchWindow& window, const IngestEnvelope& envelope) {
    if (window.first_arrival_time_ns == 0) {
        window.first_arrival_time_ns = envelope.arrival_time_ns;
    }
    window.last_arrival_time_ns = std::max(window.last_arrival_time_ns, envelope.arrival_time_ns);
    if (envelope.deadline_ns != 0) {
        if (window.earliest_deadline_ns == 0) {
            window.earliest_deadline_ns = envelope.deadline_ns;
        } else {
            window.earliest_deadline_ns = std::min(window.earliest_deadline_ns, envelope.deadline_ns);
        }
    }
}

} // namespace

BatchingPolicy::BatchingPolicy(BatchingPolicyConfig config)
    : config_(config) {}

const BatchingPolicyConfig& BatchingPolicy::config() const noexcept {
    return config_;
}

BatchingPolicyStats BatchingPolicy::stats() const noexcept {
    return stats_;
}

const std::optional<PacketBatch>& BatchingPolicy::open_batch() const noexcept {
    return open_;
}

bool BatchingPolicy::empty() const noexcept {
    return !open_ || open_->packets.empty();
}

std::vector<PacketBatch> BatchingPolicy::offer(IngestEnvelope envelope, std::uint64_t now_ns) {
    std::vector<PacketBatch> ready;
    ++stats_.packets_considered;

    if (envelope.priority == IngestPriority::normal) {
        envelope.priority = infer_ingest_priority(envelope.packet);
    }

    if (open_ && should_flush_open_batch(now_ns)) {
        ready.push_back(close(infer_batch_close_reason(*open_, config_, now_ns)));
    }

    auto candidate = evaluate(envelope, now_ns);
    if (!candidate.accepted && open_) {
        ready.push_back(close(candidate.close_reason));
    }

    if (config_.isolate_control_packets && is_control_like(envelope.packet) && open_ && !open_->packets.empty()) {
        ready.push_back(close(BatchCloseReason::priority_flush));
    }

    append(std::move(envelope));

    if (open_ && should_flush_open_batch(now_ns)) {
        ready.push_back(close(infer_batch_close_reason(*open_, config_, now_ns)));
    } else if (open_ && rank(open_->highest_priority) >= rank(config_.priority_flush_threshold)) {
        ready.push_back(close(BatchCloseReason::priority_flush));
    }

    return ready;
}

std::vector<PacketBatch> BatchingPolicy::offer_many(std::vector<IngestEnvelope> envelopes, std::uint64_t now_ns) {
    std::vector<PacketBatch> ready;
    for (auto& envelope : envelopes) {
        auto batches = offer(std::move(envelope), now_ns);
        ready.insert(ready.end(), std::make_move_iterator(batches.begin()), std::make_move_iterator(batches.end()));
    }
    return ready;
}

std::vector<PacketBatch> BatchingPolicy::flush() {
    std::vector<PacketBatch> ready;
    ++stats_.manual_flushes;
    if (open_ && !open_->packets.empty()) {
        ready.push_back(close(BatchCloseReason::manual_flush));
    }
    return ready;
}

std::vector<PacketBatch> BatchingPolicy::tick(std::uint64_t now_ns) {
    std::vector<PacketBatch> ready;
    if (open_ && should_flush_open_batch(now_ns)) {
        ready.push_back(close(infer_batch_close_reason(*open_, config_, now_ns)));
    }
    return ready;
}

void BatchingPolicy::reset() {
    open_.reset();
    stats_ = {};
}

BatchCandidate BatchingPolicy::evaluate(const IngestEnvelope& envelope, std::uint64_t now_ns) const {
    BatchCandidate candidate;
    candidate.accepted = true;
    candidate.note = "accepted";

    if (!open_ || open_->packets.empty()) {
        return candidate;
    }

    if (open_->packets.size() >= config_.max_packets) {
        candidate.accepted = false;
        candidate.close_reason = BatchCloseReason::max_packets;
        candidate.note = "batch packet limit reached";
        return candidate;
    }

    if (open_->payload_bytes + envelope.packet.payload.size() > config_.max_payload_bytes) {
        candidate.accepted = false;
        candidate.close_reason = BatchCloseReason::max_payload_bytes;
        candidate.note = "batch payload limit reached";
        return candidate;
    }

    if (open_->window.first_arrival_time_ns != 0
        && now_ns >= open_->window.first_arrival_time_ns
        && now_ns - open_->window.first_arrival_time_ns >= config_.max_age_ns) {
        candidate.accepted = false;
        candidate.close_reason = BatchCloseReason::max_age;
        candidate.note = "batch age limit reached";
        return candidate;
    }

    if (open_->window.earliest_deadline_ns != 0
        && now_ns + config_.deadline_slack_ns >= open_->window.earliest_deadline_ns) {
        candidate.accepted = false;
        candidate.close_reason = BatchCloseReason::deadline;
        candidate.note = "batch deadline reached";
        return candidate;
    }

    if (config_.preserve_device_order && conflicts_with_device_order(envelope)) {
        candidate.accepted = false;
        candidate.close_reason = BatchCloseReason::manual_flush;
        candidate.note = "device sequence would move backwards";
        return candidate;
    }

    if (config_.isolate_control_packets) {
        const bool open_is_control = is_control_like(open_->packets.front().packet);
        const bool incoming_is_control = is_control_like(envelope.packet);
        if (open_is_control != incoming_is_control) {
            candidate.accepted = false;
            candidate.close_reason = BatchCloseReason::priority_flush;
            candidate.note = "control packets are isolated";
            return candidate;
        }
    }

    return candidate;
}

bool BatchingPolicy::should_flush_open_batch(std::uint64_t now_ns) const {
    if (!open_ || open_->packets.empty()) {
        return false;
    }
    const auto reason = infer_batch_close_reason(*open_, config_, now_ns);
    return reason != BatchCloseReason::none;
}

bool BatchingPolicy::conflicts_with_device_order(const IngestEnvelope& envelope) const {
    if (!open_) {
        return false;
    }
    for (const auto& existing : open_->packets) {
        if (existing.packet.device != envelope.packet.device) {
            continue;
        }
        if (existing.packet.sequence > envelope.packet.sequence) {
            return true;
        }
    }
    return false;
}

void BatchingPolicy::append(IngestEnvelope envelope) {
    if (!open_) {
        open_ = PacketBatch{};
    }
    update_window(open_->window, envelope);
    open_->payload_bytes += envelope.packet.payload.size();
    if (rank(envelope.priority) > rank(open_->highest_priority)) {
        open_->highest_priority = envelope.priority;
    }
    open_->packets.push_back(std::move(envelope));
    ++stats_.packets_batched;
}

PacketBatch BatchingPolicy::close(BatchCloseReason reason) {
    PacketBatch batch = std::move(*open_);
    batch.close_reason = reason;
    open_.reset();
    ++stats_.batches_closed;
    count_close(reason);
    return batch;
}

void BatchingPolicy::count_close(BatchCloseReason reason) {
    switch (reason) {
    case BatchCloseReason::none:
        break;
    case BatchCloseReason::max_packets:
        ++stats_.max_packet_closures;
        break;
    case BatchCloseReason::max_payload_bytes:
        ++stats_.byte_limit_closures;
        break;
    case BatchCloseReason::max_age:
        ++stats_.age_closures;
        break;
    case BatchCloseReason::deadline:
        ++stats_.deadline_closures;
        break;
    case BatchCloseReason::priority_flush:
        ++stats_.priority_closures;
        break;
    case BatchCloseReason::manual_flush:
        break;
    }
}

BatchCloseReason infer_batch_close_reason(const PacketBatch& batch,
                                          const BatchingPolicyConfig& config,
                                          std::uint64_t now_ns) {
    if (batch.packets.empty()) {
        return BatchCloseReason::none;
    }
    if (batch.packets.size() >= config.max_packets) {
        return BatchCloseReason::max_packets;
    }
    if (batch.payload_bytes >= config.max_payload_bytes) {
        return BatchCloseReason::max_payload_bytes;
    }
    if (batch.window.first_arrival_time_ns != 0
        && now_ns >= batch.window.first_arrival_time_ns
        && now_ns - batch.window.first_arrival_time_ns >= config.max_age_ns) {
        return BatchCloseReason::max_age;
    }
    if (batch.window.earliest_deadline_ns != 0
        && now_ns + config.deadline_slack_ns >= batch.window.earliest_deadline_ns) {
        return BatchCloseReason::deadline;
    }
    if (rank(batch.highest_priority) >= rank(config.priority_flush_threshold)) {
        return BatchCloseReason::priority_flush;
    }
    return BatchCloseReason::none;
}

std::string batch_close_reason_name(BatchCloseReason reason) {
    switch (reason) {
    case BatchCloseReason::none:
        return "none";
    case BatchCloseReason::max_packets:
        return "max_packets";
    case BatchCloseReason::max_payload_bytes:
        return "max_payload_bytes";
    case BatchCloseReason::max_age:
        return "max_age";
    case BatchCloseReason::deadline:
        return "deadline";
    case BatchCloseReason::priority_flush:
        return "priority_flush";
    case BatchCloseReason::manual_flush:
        return "manual_flush";
    }
    return "unknown";
}

std::string render_packet_batch(const PacketBatch& batch) {
    std::ostringstream out;
    out << "packet_batch\n"
        << "  packets: "
        << batch.packets.size()
        << "\n"
        << "  payload_bytes: "
        << batch.payload_bytes
        << "\n"
        << "  highest_priority: "
        << ingest_priority_name(batch.highest_priority)
        << "\n"
        << "  close_reason: "
        << batch_close_reason_name(batch.close_reason)
        << "\n"
        << "  first_arrival_time_ns: "
        << batch.window.first_arrival_time_ns
        << "\n"
        << "  last_arrival_time_ns: "
        << batch.window.last_arrival_time_ns
        << "\n";
    if (batch.window.earliest_deadline_ns != 0) {
        out << "  earliest_deadline_ns: "
            << batch.window.earliest_deadline_ns
            << "\n";
    }
    for (const auto& envelope : batch.packets) {
        out << "  packet: device="
            << envelope.packet.device
            << " sequence="
            << envelope.packet.sequence
            << " priority="
            << ingest_priority_name(envelope.priority)
            << "\n";
    }
    return out.str();
}

std::string render_batching_stats(const BatchingPolicyStats& stats) {
    std::ostringstream out;
    out << "batching_policy_stats\n"
        << "  packets_considered: "
        << stats.packets_considered
        << "\n"
        << "  packets_batched: "
        << stats.packets_batched
        << "\n"
        << "  batches_closed: "
        << stats.batches_closed
        << "\n"
        << "  manual_flushes: "
        << stats.manual_flushes
        << "\n"
        << "  max_packet_closures: "
        << stats.max_packet_closures
        << "\n"
        << "  byte_limit_closures: "
        << stats.byte_limit_closures
        << "\n"
        << "  age_closures: "
        << stats.age_closures
        << "\n"
        << "  deadline_closures: "
        << stats.deadline_closures
        << "\n"
        << "  priority_closures: "
        << stats.priority_closures
        << "\n";
    return out.str();
}

} // namespace aethon::runtime
