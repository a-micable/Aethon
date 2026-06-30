#include "aethon/runtime/ingest_queue.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::runtime {
namespace {

std::uint8_t priority_rank(IngestPriority priority) {
    return static_cast<std::uint8_t>(priority);
}

bool deadline_less(const IngestEnvelope& lhs, const IngestEnvelope& rhs) {
    if (lhs.deadline_ns == 0 && rhs.deadline_ns == 0) {
        return false;
    }
    if (lhs.deadline_ns == 0) {
        return false;
    }
    if (rhs.deadline_ns == 0) {
        return true;
    }
    return lhs.deadline_ns < rhs.deadline_ns;
}

std::string packet_label(const protocol::Packet& packet) {
    std::ostringstream out;
    out << "device="
        << packet.device
        << " sequence="
        << packet.sequence;
    return out.str();
}

} // namespace

IngestQueue::IngestQueue(IngestQueueConfig config)
    : config_(config) {}

void IngestQueue::subscribe(RuntimeEventHandler handler) {
    dispatcher_.subscribe(std::move(handler));
}

void IngestQueue::close() {
    closed_ = true;
}

void IngestQueue::reopen() {
    closed_ = false;
}

void IngestQueue::clear() {
    queue_.clear();
    devices_.clear();
    stats_.queued_packets = 0;
    stats_.queued_payload_bytes = 0;
}

bool IngestQueue::closed() const noexcept {
    return closed_;
}

const IngestQueueConfig& IngestQueue::config() const noexcept {
    return config_;
}

IngestQueueStats IngestQueue::stats() const noexcept {
    auto copy = stats_;
    copy.queued_packets = queue_.size();
    copy.queued_payload_bytes = stats_.queued_payload_bytes;
    return copy;
}

std::vector<IngestDeviceSnapshot> IngestQueue::devices() const {
    std::vector<IngestDeviceSnapshot> snapshots;
    snapshots.reserve(devices_.size());
    for (const auto& [device, state] : devices_) {
        snapshots.push_back(IngestDeviceSnapshot{
            device,
            state.queued_packets,
            state.queued_payload_bytes,
            state.newest_sequence,
            state.last_arrival_time_ns,
            state.highest_priority,
        });
    }
    return snapshots;
}

std::size_t IngestQueue::size() const noexcept {
    return queue_.size();
}

std::size_t IngestQueue::payload_bytes() const noexcept {
    return stats_.queued_payload_bytes;
}

bool IngestQueue::empty() const noexcept {
    return queue_.empty();
}

IngestAdmission IngestQueue::push(IngestEnvelope envelope) {
    if (closed_) {
        ++stats_.rejected;
        ++stats_.closed_rejections;
        return reject(IngestAdmissionStatus::rejected_closed, "ingest queue is closed");
    }
    if (config_.reject_empty_payload && envelope.packet.payload.empty()) {
        ++stats_.rejected;
        ++stats_.payload_rejections;
        return reject(IngestAdmissionStatus::rejected_empty_payload, "packet payload is empty");
    }
    if (envelope.packet.payload.size() > config_.max_packet_payload_bytes) {
        ++stats_.rejected;
        ++stats_.payload_rejections;
        return reject(IngestAdmissionStatus::rejected_payload_too_large, "packet payload exceeds per-packet limit");
    }

    if (envelope.priority == IngestPriority::normal) {
        envelope.priority = infer_ingest_priority(envelope.packet);
    }

    std::optional<protocol::Packet> evicted;
    auto status = IngestAdmissionStatus::accepted;
    if (would_exceed_capacity(envelope)) {
        auto victim = select_overflow_victim(envelope);
        if (victim == queue_.end()) {
            ++stats_.rejected;
            ++stats_.capacity_rejections;
            return reject(IngestAdmissionStatus::rejected_queue_full, "ingest queue capacity reached");
        }
        evicted = victim->packet;
        remove_at(victim);
        ++stats_.evicted;
        status = config_.overflow_policy == IngestOverflowPolicy::replace_same_device
            ? IngestAdmissionStatus::replaced_older_packet
            : IngestAdmissionStatus::accepted;
    }

    insert(std::move(envelope));
    ++stats_.accepted;
    update_high_water();

    IngestAdmission admission;
    admission.status = status;
    admission.reason = status == IngestAdmissionStatus::accepted ? "accepted" : "accepted after replacing older packet";
    admission.evicted = std::move(evicted);
    admission.queue_size = queue_.size();
    admission.payload_bytes = stats_.queued_payload_bytes;
    return admission;
}

std::optional<IngestEnvelope> IngestQueue::peek() const {
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto selected = select_next();
    if (selected == queue_.end()) {
        return std::nullopt;
    }
    return *selected;
}

std::optional<IngestEnvelope> IngestQueue::pop() {
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto selected = select_next();
    if (selected == queue_.end()) {
        return std::nullopt;
    }
    auto envelope = *selected;
    remove_at(selected);
    ++stats_.drained;
    publish(RuntimeEventKind::packet_received, envelope, "ingest packet drained");
    return envelope;
}

IngestDrainResult IngestQueue::drain(std::size_t max_packets) {
    IngestDrainResult result;
    result.packets.reserve(std::min(max_packets, queue_.size()));
    while (result.packets.size() < max_packets) {
        auto next = pop();
        if (!next) {
            break;
        }
        result.packets.push_back(std::move(*next));
    }
    result.remaining_packets = queue_.size();
    result.remaining_payload_bytes = stats_.queued_payload_bytes;
    return result;
}

IngestDrainResult IngestQueue::drain_payload_budget(std::size_t max_payload_bytes) {
    IngestDrainResult result;
    std::size_t used = 0;
    while (!queue_.empty()) {
        auto next = peek();
        if (!next) {
            break;
        }
        const auto payload_size = next->packet.payload.size();
        if (!result.packets.empty() && used + payload_size > max_payload_bytes) {
            break;
        }
        if (result.packets.empty() && payload_size > max_payload_bytes) {
            break;
        }
        auto removed = pop();
        if (!removed) {
            break;
        }
        used += payload_size;
        result.packets.push_back(std::move(*removed));
    }
    result.remaining_packets = queue_.size();
    result.remaining_payload_bytes = stats_.queued_payload_bytes;
    return result;
}

IngestAdmission IngestQueue::reject(IngestAdmissionStatus status, std::string reason) const {
    IngestAdmission admission;
    admission.status = status;
    admission.reason = std::move(reason);
    admission.queue_size = queue_.size();
    admission.payload_bytes = stats_.queued_payload_bytes;
    return admission;
}

bool IngestQueue::would_exceed_capacity(const IngestEnvelope& envelope) const noexcept {
    const auto payload_size = envelope.packet.payload.size();
    if (queue_.size() >= config_.max_packets) {
        return true;
    }
    if (stats_.queued_payload_bytes + payload_size > config_.max_payload_bytes) {
        return true;
    }
    return false;
}

std::deque<IngestEnvelope>::iterator IngestQueue::select_overflow_victim(const IngestEnvelope& incoming) {
    if (queue_.empty()) {
        return queue_.end();
    }

    switch (config_.overflow_policy) {
    case IngestOverflowPolicy::reject_new:
        return queue_.end();
    case IngestOverflowPolicy::drop_oldest:
        return queue_.begin();
    case IngestOverflowPolicy::drop_lowest_priority:
        return std::min_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
            if (priority_rank(lhs.priority) != priority_rank(rhs.priority)) {
                return priority_rank(lhs.priority) < priority_rank(rhs.priority);
            }
            return lhs.arrival_time_ns < rhs.arrival_time_ns;
        });
    case IngestOverflowPolicy::replace_same_device: {
        auto candidate = queue_.end();
        for (auto iter = queue_.begin(); iter != queue_.end(); ++iter) {
            if (iter->packet.device != incoming.packet.device) {
                continue;
            }
            if (candidate == queue_.end() || iter->arrival_time_ns < candidate->arrival_time_ns) {
                candidate = iter;
            }
        }
        return candidate;
    }
    }
    return queue_.end();
}

std::deque<IngestEnvelope>::iterator IngestQueue::select_next() {
    if (queue_.empty()) {
        return queue_.end();
    }
    switch (config_.drain_mode) {
    case IngestDrainMode::fifo:
        return queue_.begin();
    case IngestDrainMode::highest_priority_first:
        return std::max_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
            if (priority_rank(lhs.priority) != priority_rank(rhs.priority)) {
                return priority_rank(lhs.priority) < priority_rank(rhs.priority);
            }
            return lhs.arrival_time_ns > rhs.arrival_time_ns;
        });
    case IngestDrainMode::earliest_deadline_first:
        return std::min_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
            if (deadline_less(lhs, rhs)) {
                return true;
            }
            if (deadline_less(rhs, lhs)) {
                return false;
            }
            return lhs.arrival_time_ns < rhs.arrival_time_ns;
        });
    }
    return queue_.begin();
}

std::deque<IngestEnvelope>::const_iterator IngestQueue::select_next() const {
    if (queue_.empty()) {
        return queue_.end();
    }
    switch (config_.drain_mode) {
    case IngestDrainMode::fifo:
        return queue_.begin();
    case IngestDrainMode::highest_priority_first:
        return std::max_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
            if (priority_rank(lhs.priority) != priority_rank(rhs.priority)) {
                return priority_rank(lhs.priority) < priority_rank(rhs.priority);
            }
            return lhs.arrival_time_ns > rhs.arrival_time_ns;
        });
    case IngestDrainMode::earliest_deadline_first:
        return std::min_element(queue_.begin(), queue_.end(), [](const auto& lhs, const auto& rhs) {
            if (deadline_less(lhs, rhs)) {
                return true;
            }
            if (deadline_less(rhs, lhs)) {
                return false;
            }
            return lhs.arrival_time_ns < rhs.arrival_time_ns;
        });
    }
    return queue_.begin();
}

void IngestQueue::insert(IngestEnvelope envelope) {
    stats_.queued_payload_bytes += envelope.packet.payload.size();
    add_device_state(envelope);
    if (config_.publish_events) {
        publish(RuntimeEventKind::packet_received, envelope, "ingest packet accepted");
    }
    queue_.push_back(std::move(envelope));
    stats_.queued_packets = queue_.size();
}

void IngestQueue::remove_at(std::deque<IngestEnvelope>::iterator iter) {
    if (iter == queue_.end()) {
        return;
    }
    const auto envelope = *iter;
    if (stats_.queued_payload_bytes >= envelope.packet.payload.size()) {
        stats_.queued_payload_bytes -= envelope.packet.payload.size();
    } else {
        stats_.queued_payload_bytes = 0;
    }
    subtract_device_state(envelope);
    queue_.erase(iter);
    stats_.queued_packets = queue_.size();
}

void IngestQueue::add_device_state(const IngestEnvelope& envelope) {
    auto& state = devices_[envelope.packet.device];
    ++state.queued_packets;
    state.queued_payload_bytes += envelope.packet.payload.size();
    state.newest_sequence = std::max<std::uint64_t>(state.newest_sequence, envelope.packet.sequence);
    state.last_arrival_time_ns = std::max(state.last_arrival_time_ns, envelope.arrival_time_ns);
    if (priority_rank(envelope.priority) > priority_rank(state.highest_priority)) {
        state.highest_priority = envelope.priority;
    }
}

void IngestQueue::subtract_device_state(const IngestEnvelope& envelope) {
    auto found = devices_.find(envelope.packet.device);
    if (found == devices_.end()) {
        return;
    }
    auto& state = found->second;
    if (state.queued_packets > 0) {
        --state.queued_packets;
    }
    if (state.queued_payload_bytes >= envelope.packet.payload.size()) {
        state.queued_payload_bytes -= envelope.packet.payload.size();
    } else {
        state.queued_payload_bytes = 0;
    }
    if (state.queued_packets == 0) {
        devices_.erase(found);
        return;
    }
    if (state.highest_priority == envelope.priority
        || state.newest_sequence == envelope.packet.sequence
        || state.last_arrival_time_ns == envelope.arrival_time_ns) {
        rebuild_device_state(envelope.packet.device);
    }
}

void IngestQueue::rebuild_device_state(protocol::DeviceId device) {
    DeviceState rebuilt;
    for (const auto& envelope : queue_) {
        if (envelope.packet.device != device) {
            continue;
        }
        ++rebuilt.queued_packets;
        rebuilt.queued_payload_bytes += envelope.packet.payload.size();
        rebuilt.newest_sequence = std::max<std::uint64_t>(rebuilt.newest_sequence, envelope.packet.sequence);
        rebuilt.last_arrival_time_ns = std::max(rebuilt.last_arrival_time_ns, envelope.arrival_time_ns);
        if (priority_rank(envelope.priority) > priority_rank(rebuilt.highest_priority)) {
            rebuilt.highest_priority = envelope.priority;
        }
    }
    if (rebuilt.queued_packets == 0) {
        devices_.erase(device);
    } else {
        devices_[device] = rebuilt;
    }
}

void IngestQueue::update_high_water() {
    stats_.high_water_packets = std::max(stats_.high_water_packets, queue_.size());
    stats_.high_water_payload_bytes = std::max(stats_.high_water_payload_bytes, stats_.queued_payload_bytes);
}

void IngestQueue::publish(RuntimeEventKind kind, const IngestEnvelope& envelope, std::string message) const {
    if (!config_.publish_events) {
        return;
    }
    RuntimeEvent event;
    event.kind = kind;
    event.time_ns = envelope.arrival_time_ns;
    event.packet = envelope.packet;
    event.message = std::move(message);
    dispatcher_.publish(event);
}

IngestPriority infer_ingest_priority(const protocol::Packet& packet) {
    if (packet.route && packet.route->priority >= 200) {
        return IngestPriority::critical;
    }
    if (packet.route && packet.route->priority >= 128) {
        return IngestPriority::high;
    }
    switch (packet.kind) {
    case protocol::PacketKind::control:
        return IngestPriority::critical;
    case protocol::PacketKind::heartbeat:
    case protocol::PacketKind::capabilities:
        return IngestPriority::high;
    case protocol::PacketKind::spectrum:
        return IngestPriority::normal;
    case protocol::PacketKind::observation:
        break;
    }
    return IngestPriority::normal;
}

std::string ingest_priority_name(IngestPriority priority) {
    switch (priority) {
    case IngestPriority::low:
        return "low";
    case IngestPriority::normal:
        return "normal";
    case IngestPriority::high:
        return "high";
    case IngestPriority::critical:
        return "critical";
    }
    return "unknown";
}

std::string ingest_admission_status_name(IngestAdmissionStatus status) {
    switch (status) {
    case IngestAdmissionStatus::accepted:
        return "accepted";
    case IngestAdmissionStatus::rejected_closed:
        return "rejected_closed";
    case IngestAdmissionStatus::rejected_empty_payload:
        return "rejected_empty_payload";
    case IngestAdmissionStatus::rejected_payload_too_large:
        return "rejected_payload_too_large";
    case IngestAdmissionStatus::rejected_queue_full:
        return "rejected_queue_full";
    case IngestAdmissionStatus::replaced_older_packet:
        return "replaced_older_packet";
    }
    return "unknown";
}

std::string ingest_overflow_policy_name(IngestOverflowPolicy policy) {
    switch (policy) {
    case IngestOverflowPolicy::reject_new:
        return "reject_new";
    case IngestOverflowPolicy::drop_oldest:
        return "drop_oldest";
    case IngestOverflowPolicy::drop_lowest_priority:
        return "drop_lowest_priority";
    case IngestOverflowPolicy::replace_same_device:
        return "replace_same_device";
    }
    return "unknown";
}

std::string ingest_drain_mode_name(IngestDrainMode mode) {
    switch (mode) {
    case IngestDrainMode::fifo:
        return "fifo";
    case IngestDrainMode::highest_priority_first:
        return "highest_priority_first";
    case IngestDrainMode::earliest_deadline_first:
        return "earliest_deadline_first";
    }
    return "unknown";
}

std::string render_ingest_admission(const IngestAdmission& admission) {
    std::ostringstream out;
    out << "ingest_admission\n"
        << "  status: "
        << ingest_admission_status_name(admission.status)
        << "\n"
        << "  reason: "
        << admission.reason
        << "\n"
        << "  queue_size: "
        << admission.queue_size
        << "\n"
        << "  payload_bytes: "
        << admission.payload_bytes
        << "\n";
    if (admission.evicted) {
        out << "  evicted: "
            << packet_label(*admission.evicted)
            << "\n";
    }
    return out.str();
}

std::string render_ingest_queue_stats(const IngestQueueStats& stats) {
    std::ostringstream out;
    out << "ingest_queue_stats\n"
        << "  accepted: "
        << stats.accepted
        << "\n"
        << "  rejected: "
        << stats.rejected
        << "\n"
        << "  evicted: "
        << stats.evicted
        << "\n"
        << "  drained: "
        << stats.drained
        << "\n"
        << "  queued_packets: "
        << stats.queued_packets
        << "\n"
        << "  queued_payload_bytes: "
        << stats.queued_payload_bytes
        << "\n"
        << "  high_water_packets: "
        << stats.high_water_packets
        << "\n"
        << "  high_water_payload_bytes: "
        << stats.high_water_payload_bytes
        << "\n";
    return out.str();
}

} // namespace aethon::runtime
