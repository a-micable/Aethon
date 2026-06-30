#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::stream {

enum class SessionEventKind {
    opened,
    packet_seen,
    duplicate_sequence,
    sequence_gap,
    timestamp_regression,
    idle_timeout,
    closed,
};

struct SessionEvent {
    SessionEventKind kind = SessionEventKind::packet_seen;
    protocol::DeviceId device = 0;
    std::uint32_t sequence = 0;
    std::uint64_t timestamp_ns = 0;
    std::uint32_t expected_sequence = 0;
    std::string message;
};

struct SessionState {
    protocol::DeviceId device = 0;
    std::uint64_t opened_at_ns = 0;
    std::uint64_t last_seen_ns = 0;
    std::uint64_t last_packet_time_ns = 0;
    std::uint32_t first_sequence = 0;
    std::uint32_t last_sequence = 0;
    std::uint64_t packets = 0;
    std::uint64_t duplicates = 0;
    std::uint64_t gaps = 0;
    std::uint64_t timestamp_regressions = 0;
    bool open = false;
};

struct SessionTrackerOptions {
    std::uint64_t idle_timeout_ns = 30'000'000'000ULL;
    std::uint32_t max_gap_without_warning = 4;
};

struct SessionTrackerSnapshot {
    std::vector<SessionState> sessions;
    std::uint64_t total_packets = 0;
    std::uint64_t total_duplicates = 0;
    std::uint64_t total_gaps = 0;
    std::uint64_t total_timestamp_regressions = 0;
};

class SessionTracker {
public:
    explicit SessionTracker(SessionTrackerOptions options = {});

    [[nodiscard]] std::vector<SessionEvent> observe(const protocol::Packet& packet,
                                                    std::uint64_t arrival_time_ns);

    [[nodiscard]] std::vector<SessionEvent> expire_idle(std::uint64_t now_ns);

    [[nodiscard]] std::optional<SessionState> find(protocol::DeviceId device) const;

    [[nodiscard]] SessionTrackerSnapshot snapshot() const;

    void clear();

private:
    SessionTrackerOptions options_;
    std::map<protocol::DeviceId, SessionState> sessions_;
};

[[nodiscard]] std::string session_event_name(SessionEventKind kind);
[[nodiscard]] std::string render_session_event(const SessionEvent& event);
[[nodiscard]] std::string render_session_snapshot(const SessionTrackerSnapshot& snapshot);

} // namespace aethon::stream
