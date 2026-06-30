#include "aethon/control/ack_tracker.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::control {
namespace {

bool accepted_status(AckStatus status) {
    return status == AckStatus::accepted;
}

bool rejected_status(AckStatus status) {
    return status == AckStatus::rejected
        || status == AckStatus::malformed
        || status == AckStatus::unknown_command;
}

std::string ack_message(const CommandAck& ack) {
    if (!ack.message.empty()) {
        return ack.message;
    }
    std::ostringstream out;
    out << "ack "
        << ack_status_name(ack.status)
        << " for command "
        << ack.command_id;
    return out.str();
}

} // namespace

AckTrackerStats AckTracker::stats() const noexcept {
    return stats_;
}

std::size_t AckTracker::size() const noexcept {
    return records_.size();
}

bool AckTracker::empty() const noexcept {
    return records_.empty();
}

std::optional<AckExpectationSnapshot> AckTracker::find(CommandId command_id) const {
    auto found = records_.find(command_id);
    if (found == records_.end()) {
        return std::nullopt;
    }
    return snapshot(found->second);
}

std::vector<AckExpectationSnapshot> AckTracker::snapshots() const {
    std::vector<AckExpectationSnapshot> result;
    result.reserve(records_.size());
    for (const auto& [unused, record] : records_) {
        (void)unused;
        result.push_back(snapshot(record));
    }
    return result;
}

std::vector<AckExpectationSnapshot> AckTracker::waiting_for_device(protocol::DeviceId device) const {
    std::vector<AckExpectationSnapshot> result;
    for (const auto& [unused, record] : records_) {
        (void)unused;
        if (record.expectation.device == device && record.state == AckExpectationState::waiting) {
            result.push_back(snapshot(record));
        }
    }
    return result;
}

bool AckTracker::expect(AckExpectation expectation) {
    if (expectation.command_id == 0) {
        return false;
    }
    auto found = records_.find(expectation.command_id);
    if (found != records_.end() && !terminal(found->second.state)) {
        return false;
    }
    if (expectation.required_accepts == 0) {
        expectation.required_accepts = 1;
    }

    Record record;
    record.expectation = std::move(expectation);
    const auto id = record.expectation.command_id;
    records_[id] = std::move(record);
    ++stats_.registered;
    return true;
}

AckObservation AckTracker::observe(const CommandAck& ack) {
    auto found = records_.find(ack.command_id);
    if (found == records_.end()) {
        ++stats_.unexpected;
        return AckObservation{
            AckObservationKind::unexpected,
            ack.command_id,
            ack.device,
            ack.time_ns,
            "ack did not match any expectation",
        };
    }

    auto& record = found->second;
    auto observation = classify(record, ack);
    record_observation(record, observation);

    switch (observation.kind) {
    case AckObservationKind::matched:
        ++stats_.matched;
        break;
    case AckObservationKind::duplicate:
        ++stats_.duplicates;
        break;
    case AckObservationKind::unexpected:
        ++stats_.unexpected;
        break;
    case AckObservationKind::stale:
        ++stats_.stale;
        break;
    case AckObservationKind::rejected:
        ++stats_.rejected;
        break;
    }
    return observation;
}

std::vector<AckExpectationSnapshot> AckTracker::timeouts(std::uint64_t now_ns) {
    std::vector<AckExpectationSnapshot> result;
    for (auto& [unused, record] : records_) {
        (void)unused;
        if (record.state != AckExpectationState::waiting) {
            continue;
        }
        if (record.expectation.deadline_ns != 0 && now_ns >= record.expectation.deadline_ns) {
            record.state = AckExpectationState::timed_out;
            AckObservation observation;
            observation.kind = AckObservationKind::stale;
            observation.command_id = record.expectation.command_id;
            observation.device = record.expectation.device;
            observation.time_ns = now_ns;
            observation.message = "ack expectation timed out";
            record_observation(record, observation);
            result.push_back(snapshot(record));
            ++stats_.timed_out;
        }
    }
    return result;
}

bool AckTracker::cancel(CommandId command_id, std::string message) {
    auto found = records_.find(command_id);
    if (found == records_.end()) {
        return false;
    }
    auto& record = found->second;
    if (terminal(record.state)) {
        return false;
    }
    record.state = AckExpectationState::cancelled;
    AckObservation observation;
    observation.kind = AckObservationKind::stale;
    observation.command_id = command_id;
    observation.device = record.expectation.device;
    observation.time_ns = record.last_observation_time_ns;
    observation.message = std::move(message);
    record_observation(record, observation);
    ++stats_.cancelled;
    return true;
}

void AckTracker::compact_terminal(std::size_t keep_recent) {
    if (records_.size() <= keep_recent) {
        return;
    }
    std::vector<CommandId> removable;
    removable.reserve(records_.size());
    for (const auto& [id, record] : records_) {
        if (terminal(record.state)) {
            removable.push_back(id);
        }
    }
    const auto limit = records_.size() > keep_recent ? records_.size() - keep_recent : 0;
    std::size_t removed = 0;
    for (auto id : removable) {
        if (removed >= limit) {
            break;
        }
        records_.erase(id);
        ++removed;
    }
}

void AckTracker::reset() {
    records_.clear();
    stats_ = {};
}

bool AckTracker::terminal(AckExpectationState state) const noexcept {
    return state == AckExpectationState::satisfied
        || state == AckExpectationState::timed_out
        || state == AckExpectationState::cancelled;
}

AckObservation AckTracker::classify(const Record& record, const CommandAck& ack) const {
    if (terminal(record.state)) {
        return AckObservation{
            AckObservationKind::duplicate,
            ack.command_id,
            ack.device,
            ack.time_ns,
            "ack matched a terminal expectation",
        };
    }
    if (ack.device != record.expectation.device) {
        return AckObservation{
            AckObservationKind::unexpected,
            ack.command_id,
            ack.device,
            ack.time_ns,
            "ack device did not match expectation",
        };
    }
    if (record.expectation.deadline_ns != 0 && ack.time_ns > record.expectation.deadline_ns) {
        return AckObservation{
            AckObservationKind::stale,
            ack.command_id,
            ack.device,
            ack.time_ns,
            "ack arrived after deadline",
        };
    }
    if (accepted_status(ack.status)) {
        if (record.accepts_seen >= record.expectation.required_accepts) {
            return AckObservation{
                AckObservationKind::duplicate,
                ack.command_id,
                ack.device,
                ack.time_ns,
                "ack acceptance was already satisfied",
            };
        }
        return AckObservation{
            AckObservationKind::matched,
            ack.command_id,
            ack.device,
            ack.time_ns,
            ack_message(ack),
        };
    }
    if (rejected_status(ack.status)) {
        return AckObservation{
            AckObservationKind::rejected,
            ack.command_id,
            ack.device,
            ack.time_ns,
            ack_message(ack),
        };
    }
    return AckObservation{
        AckObservationKind::matched,
        ack.command_id,
        ack.device,
        ack.time_ns,
        ack_message(ack),
    };
}

void AckTracker::record_observation(Record& record, AckObservation observation) {
    record.last_observation_time_ns = std::max(record.last_observation_time_ns, observation.time_ns);
    switch (observation.kind) {
    case AckObservationKind::matched:
        ++record.accepts_seen;
        if (record.accepts_seen >= record.expectation.required_accepts) {
            record.state = AckExpectationState::satisfied;
        }
        break;
    case AckObservationKind::rejected:
        ++record.rejects_seen;
        record.state = AckExpectationState::satisfied;
        break;
    case AckObservationKind::duplicate:
    case AckObservationKind::unexpected:
    case AckObservationKind::stale:
        break;
    }
    record.observations.push_back(std::move(observation));
}

AckExpectationSnapshot AckTracker::snapshot(const Record& record) const {
    return AckExpectationSnapshot{
        record.expectation,
        record.state,
        record.accepts_seen,
        record.rejects_seen,
        record.last_observation_time_ns,
        record.observations,
    };
}

AckExpectation expectation_from_entry(const JournalEntry& entry, std::uint64_t timeout_ns) {
    AckExpectation expectation;
    expectation.command_id = entry.command.id;
    expectation.device = entry.command.device;
    expectation.created_time_ns = entry.last_send_time_ns == 0
        ? entry.command.created_time_ns
        : entry.last_send_time_ns;
    expectation.deadline_ns = expectation.created_time_ns + timeout_ns;
    expectation.required_accepts = 1;
    expectation.label = entry.command.label;
    return expectation;
}

std::string ack_expectation_state_name(AckExpectationState state) {
    switch (state) {
    case AckExpectationState::waiting:
        return "waiting";
    case AckExpectationState::satisfied:
        return "satisfied";
    case AckExpectationState::timed_out:
        return "timed_out";
    case AckExpectationState::cancelled:
        return "cancelled";
    }
    return "unknown";
}

std::string ack_observation_kind_name(AckObservationKind kind) {
    switch (kind) {
    case AckObservationKind::matched:
        return "matched";
    case AckObservationKind::duplicate:
        return "duplicate";
    case AckObservationKind::unexpected:
        return "unexpected";
    case AckObservationKind::stale:
        return "stale";
    case AckObservationKind::rejected:
        return "rejected";
    }
    return "unknown";
}

std::string render_ack_observation(const AckObservation& observation) {
    std::ostringstream out;
    out << "ack_observation\n"
        << "  kind: "
        << ack_observation_kind_name(observation.kind)
        << "\n"
        << "  command_id: "
        << observation.command_id
        << "\n"
        << "  device: "
        << observation.device
        << "\n"
        << "  time_ns: "
        << observation.time_ns
        << "\n"
        << "  message: "
        << observation.message
        << "\n";
    return out.str();
}

std::string render_ack_tracker_stats(const AckTrackerStats& stats) {
    std::ostringstream out;
    out << "ack_tracker_stats\n"
        << "  registered: "
        << stats.registered
        << "\n"
        << "  matched: "
        << stats.matched
        << "\n"
        << "  duplicates: "
        << stats.duplicates
        << "\n"
        << "  unexpected: "
        << stats.unexpected
        << "\n"
        << "  stale: "
        << stats.stale
        << "\n"
        << "  rejected: "
        << stats.rejected
        << "\n"
        << "  timed_out: "
        << stats.timed_out
        << "\n"
        << "  cancelled: "
        << stats.cancelled
        << "\n";
    return out.str();
}

std::string render_ack_expectation_snapshot(const AckExpectationSnapshot& snapshot) {
    std::ostringstream out;
    out << "ack_expectation\n"
        << "  command_id: "
        << snapshot.expectation.command_id
        << "\n"
        << "  device: "
        << snapshot.expectation.device
        << "\n"
        << "  state: "
        << ack_expectation_state_name(snapshot.state)
        << "\n"
        << "  accepts_seen: "
        << snapshot.accepts_seen
        << "\n"
        << "  rejects_seen: "
        << snapshot.rejects_seen
        << "\n"
        << "  deadline_ns: "
        << snapshot.expectation.deadline_ns
        << "\n";
    if (!snapshot.expectation.label.empty()) {
        out << "  label: "
            << snapshot.expectation.label
            << "\n";
    }
    for (const auto& observation : snapshot.observations) {
        out << "  observation: "
            << ack_observation_kind_name(observation.kind)
            << " time_ns="
            << observation.time_ns
            << " message=\""
            << observation.message
            << "\"\n";
    }
    return out.str();
}

std::vector<AckDeviceSummary> summarize_ack_devices(const std::vector<AckExpectationSnapshot>& snapshots) {
    std::map<protocol::DeviceId, AckDeviceSummary> by_device;
    for (const auto& snapshot : snapshots) {
        auto& summary = by_device[snapshot.expectation.device];
        summary.device = snapshot.expectation.device;
        summary.observations += static_cast<std::uint32_t>(snapshot.observations.size());
        switch (snapshot.state) {
        case AckExpectationState::waiting:
            ++summary.waiting;
            break;
        case AckExpectationState::satisfied:
            ++summary.satisfied;
            break;
        case AckExpectationState::timed_out:
            ++summary.timed_out;
            break;
        case AckExpectationState::cancelled:
            ++summary.cancelled;
            break;
        }
    }

    std::vector<AckDeviceSummary> summaries;
    summaries.reserve(by_device.size());
    for (const auto& [unused, summary] : by_device) {
        (void)unused;
        summaries.push_back(summary);
    }
    return summaries;
}

std::string render_ack_device_summaries(const std::vector<AckDeviceSummary>& summaries) {
    std::ostringstream out;
    out << "ack_device_summaries\n";
    for (const auto& summary : summaries) {
        out << "  device: "
            << summary.device
            << " waiting="
            << summary.waiting
            << " satisfied="
            << summary.satisfied
            << " timed_out="
            << summary.timed_out
            << " cancelled="
            << summary.cancelled
            << " observations="
            << summary.observations
            << "\n";
    }
    return out.str();
}

} // namespace aethon::control
