#include "aethon/stream/session_tracker.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::stream {
namespace {

SessionEvent make_event(SessionEventKind kind,
                        const SessionState& state,
                        std::uint32_t sequence,
                        std::uint64_t timestamp_ns,
                        std::string message) {
    SessionEvent event;
    event.kind = kind;
    event.device = state.device;
    event.sequence = sequence;
    event.timestamp_ns = timestamp_ns;
    event.expected_sequence = state.last_sequence + 1;
    event.message = std::move(message);
    return event;
}

void append_event(std::vector<SessionEvent>& events,
                  SessionEventKind kind,
                  const SessionState& state,
                  std::uint32_t sequence,
                  std::uint64_t timestamp_ns,
                  std::string message) {
    events.push_back(make_event(
        kind,
        state,
        sequence,
        timestamp_ns,
        std::move(message)));
}

SessionState new_session(const protocol::Packet& packet, std::uint64_t arrival_time_ns) {
    SessionState state;
    state.device = packet.device;
    state.opened_at_ns = arrival_time_ns;
    state.last_seen_ns = arrival_time_ns;
    state.last_packet_time_ns = packet.timestamp_ns;
    state.first_sequence = packet.sequence;
    state.last_sequence = packet.sequence;
    state.packets = 1;
    state.open = true;
    return state;
}

bool sequence_after(std::uint32_t newer, std::uint32_t older) {
    return newer > older;
}

std::uint32_t sequence_gap(std::uint32_t newer, std::uint32_t older) {
    if (newer <= older + 1) {
        return 0;
    }
    return newer - older - 1;
}

void accumulate(SessionTrackerSnapshot& snapshot, const SessionState& state) {
    snapshot.total_packets += state.packets;
    snapshot.total_duplicates += state.duplicates;
    snapshot.total_gaps += state.gaps;
    snapshot.total_timestamp_regressions += state.timestamp_regressions;
}

} // namespace

SessionTracker::SessionTracker(SessionTrackerOptions options)
    : options_(options) {}

std::vector<SessionEvent> SessionTracker::observe(const protocol::Packet& packet,
                                                  std::uint64_t arrival_time_ns) {
    std::vector<SessionEvent> events;
    auto it = sessions_.find(packet.device);
    if (it == sessions_.end()) {
        auto [inserted, _] = sessions_.emplace(packet.device, new_session(packet, arrival_time_ns));
        append_event(
            events,
            SessionEventKind::opened,
            inserted->second,
            packet.sequence,
            packet.timestamp_ns,
            "session opened");
        append_event(
            events,
            SessionEventKind::packet_seen,
            inserted->second,
            packet.sequence,
            packet.timestamp_ns,
            "first packet observed");
        return events;
    }

    auto& state = it->second;
    if (!state.open) {
        state.open = true;
        state.opened_at_ns = arrival_time_ns;
        append_event(
            events,
            SessionEventKind::opened,
            state,
            packet.sequence,
            packet.timestamp_ns,
            "session reopened");
    }

    if (packet.sequence == state.last_sequence) {
        ++state.duplicates;
        append_event(
            events,
            SessionEventKind::duplicate_sequence,
            state,
            packet.sequence,
            packet.timestamp_ns,
            "duplicate packet sequence");
    } else if (sequence_after(packet.sequence, state.last_sequence)) {
        auto gap = sequence_gap(packet.sequence, state.last_sequence);
        if (gap > options_.max_gap_without_warning) {
            ++state.gaps;
            std::ostringstream message;
            message << "sequence gap of " << gap << " packets";
            append_event(
                events,
                SessionEventKind::sequence_gap,
                state,
                packet.sequence,
                packet.timestamp_ns,
                message.str());
        }
        state.last_sequence = packet.sequence;
    } else {
        ++state.duplicates;
        append_event(
            events,
            SessionEventKind::duplicate_sequence,
            state,
            packet.sequence,
            packet.timestamp_ns,
            "late or repeated packet sequence");
    }

    if (packet.timestamp_ns < state.last_packet_time_ns) {
        ++state.timestamp_regressions;
        append_event(
            events,
            SessionEventKind::timestamp_regression,
            state,
            packet.sequence,
            packet.timestamp_ns,
            "packet timestamp regressed");
    } else {
        state.last_packet_time_ns = packet.timestamp_ns;
    }

    state.last_seen_ns = arrival_time_ns;
    ++state.packets;
    append_event(
        events,
        SessionEventKind::packet_seen,
        state,
        packet.sequence,
        packet.timestamp_ns,
        "packet observed");
    return events;
}

std::vector<SessionEvent> SessionTracker::expire_idle(std::uint64_t now_ns) {
    std::vector<SessionEvent> events;
    for (auto& [device, state] : sessions_) {
        (void)device;
        if (!state.open) {
            continue;
        }
        if (now_ns < state.last_seen_ns) {
            continue;
        }
        auto idle_for = now_ns - state.last_seen_ns;
        if (idle_for < options_.idle_timeout_ns) {
            continue;
        }
        state.open = false;
        std::ostringstream message;
        message << "session idle for " << idle_for << " ns";
        append_event(
            events,
            SessionEventKind::idle_timeout,
            state,
            state.last_sequence,
            state.last_packet_time_ns,
            message.str());
        append_event(
            events,
            SessionEventKind::closed,
            state,
            state.last_sequence,
            state.last_packet_time_ns,
            "session closed after idle timeout");
    }
    return events;
}

std::optional<SessionState> SessionTracker::find(protocol::DeviceId device) const {
    auto it = sessions_.find(device);
    if (it == sessions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

SessionTrackerSnapshot SessionTracker::snapshot() const {
    SessionTrackerSnapshot snapshot;
    snapshot.sessions.reserve(sessions_.size());
    for (const auto& [device, state] : sessions_) {
        (void)device;
        snapshot.sessions.push_back(state);
        accumulate(snapshot, state);
    }
    std::sort(
        snapshot.sessions.begin(),
        snapshot.sessions.end(),
        [](const SessionState& left, const SessionState& right) {
            return left.device < right.device;
        });
    return snapshot;
}

void SessionTracker::clear() {
    sessions_.clear();
}

std::string session_event_name(SessionEventKind kind) {
    switch (kind) {
    case SessionEventKind::opened:
        return "opened";
    case SessionEventKind::packet_seen:
        return "packet_seen";
    case SessionEventKind::duplicate_sequence:
        return "duplicate_sequence";
    case SessionEventKind::sequence_gap:
        return "sequence_gap";
    case SessionEventKind::timestamp_regression:
        return "timestamp_regression";
    case SessionEventKind::idle_timeout:
        return "idle_timeout";
    case SessionEventKind::closed:
        return "closed";
    }
    return "unknown";
}

std::string render_session_event(const SessionEvent& event) {
    std::ostringstream out;
    out << "session_event "
        << session_event_name(event.kind)
        << " device="
        << event.device
        << " sequence="
        << event.sequence
        << " expected="
        << event.expected_sequence
        << " timestamp_ns="
        << event.timestamp_ns;
    if (!event.message.empty()) {
        out << " message=\""
            << event.message
            << "\"";
    }
    return out.str();
}

std::string render_session_snapshot(const SessionTrackerSnapshot& snapshot) {
    std::ostringstream out;
    out << "session_snapshot\n"
        << "  sessions: "
        << snapshot.sessions.size()
        << "\n"
        << "  packets: "
        << snapshot.total_packets
        << "\n"
        << "  duplicates: "
        << snapshot.total_duplicates
        << "\n"
        << "  gaps: "
        << snapshot.total_gaps
        << "\n"
        << "  timestamp_regressions: "
        << snapshot.total_timestamp_regressions
        << "\n";
    for (const auto& session : snapshot.sessions) {
        out << "  device: "
            << session.device
            << " open="
            << (session.open ? "yes" : "no")
            << " packets="
            << session.packets
            << " first_sequence="
            << session.first_sequence
            << " last_sequence="
            << session.last_sequence
            << " last_seen_ns="
            << session.last_seen_ns
            << "\n";
    }
    return out.str();
}

} // namespace aethon::stream
