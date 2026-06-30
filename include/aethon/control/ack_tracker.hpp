#pragma once

#include "aethon/control/command_journal.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::control {

enum class AckExpectationState : std::uint8_t {
    waiting,
    satisfied,
    timed_out,
    cancelled,
};

enum class AckObservationKind : std::uint8_t {
    matched,
    duplicate,
    unexpected,
    stale,
    rejected,
};

struct AckExpectation {
    CommandId command_id = 0;
    protocol::DeviceId device = 0;
    std::uint64_t created_time_ns = 0;
    std::uint64_t deadline_ns = 0;
    std::uint32_t required_accepts = 1;
    std::string label;
};

struct AckObservation {
    AckObservationKind kind = AckObservationKind::unexpected;
    CommandId command_id = 0;
    protocol::DeviceId device = 0;
    std::uint64_t time_ns = 0;
    std::string message;
};

struct AckExpectationSnapshot {
    AckExpectation expectation;
    AckExpectationState state = AckExpectationState::waiting;
    std::uint32_t accepts_seen = 0;
    std::uint32_t rejects_seen = 0;
    std::uint64_t last_observation_time_ns = 0;
    std::vector<AckObservation> observations;
};

struct AckTrackerStats {
    std::uint64_t registered = 0;
    std::uint64_t matched = 0;
    std::uint64_t duplicates = 0;
    std::uint64_t unexpected = 0;
    std::uint64_t stale = 0;
    std::uint64_t rejected = 0;
    std::uint64_t timed_out = 0;
    std::uint64_t cancelled = 0;
};

struct AckDeviceSummary {
    protocol::DeviceId device = 0;
    std::uint32_t waiting = 0;
    std::uint32_t satisfied = 0;
    std::uint32_t timed_out = 0;
    std::uint32_t cancelled = 0;
    std::uint32_t observations = 0;
};

class AckTracker {
public:
    AckTracker() = default;

    [[nodiscard]] AckTrackerStats stats() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::optional<AckExpectationSnapshot> find(CommandId command_id) const;
    [[nodiscard]] std::vector<AckExpectationSnapshot> snapshots() const;
    [[nodiscard]] std::vector<AckExpectationSnapshot> waiting_for_device(protocol::DeviceId device) const;

    [[nodiscard]] bool expect(AckExpectation expectation);
    [[nodiscard]] AckObservation observe(const CommandAck& ack);
    [[nodiscard]] std::vector<AckExpectationSnapshot> timeouts(std::uint64_t now_ns);
    [[nodiscard]] bool cancel(CommandId command_id, std::string message);
    void compact_terminal(std::size_t keep_recent);
    void reset();

private:
    struct Record {
        AckExpectation expectation;
        AckExpectationState state = AckExpectationState::waiting;
        std::uint32_t accepts_seen = 0;
        std::uint32_t rejects_seen = 0;
        std::uint64_t last_observation_time_ns = 0;
        std::vector<AckObservation> observations;
    };

    [[nodiscard]] bool terminal(AckExpectationState state) const noexcept;
    [[nodiscard]] AckObservation classify(const Record& record, const CommandAck& ack) const;
    void record_observation(Record& record, AckObservation observation);
    [[nodiscard]] AckExpectationSnapshot snapshot(const Record& record) const;

    std::map<CommandId, Record> records_;
    AckTrackerStats stats_;
};

[[nodiscard]] AckExpectation expectation_from_entry(const JournalEntry& entry, std::uint64_t timeout_ns);
[[nodiscard]] std::string ack_expectation_state_name(AckExpectationState state);
[[nodiscard]] std::string ack_observation_kind_name(AckObservationKind kind);
[[nodiscard]] std::string render_ack_observation(const AckObservation& observation);
[[nodiscard]] std::string render_ack_tracker_stats(const AckTrackerStats& stats);
[[nodiscard]] std::string render_ack_expectation_snapshot(const AckExpectationSnapshot& snapshot);
[[nodiscard]] std::vector<AckDeviceSummary> summarize_ack_devices(const std::vector<AckExpectationSnapshot>& snapshots);
[[nodiscard]] std::string render_ack_device_summaries(const std::vector<AckDeviceSummary>& summaries);

} // namespace aethon::control
