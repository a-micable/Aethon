#include "aethon/telemetry/window.hpp"

#include <sstream>
#include <utility>

namespace aethon::telemetry {
namespace {

ScalarObservation merge_observations(const std::deque<WindowEntry>& entries) {
    ScalarObservation merged;
    if (entries.empty()) {
        return merged;
    }
    merged.sample_time_ns = entries.back().observation.sample_time_ns;
    merged.sample_rate_hz = entries.back().observation.sample_rate_hz;
    for (const auto& entry : entries) {
        merged.readings.insert(
            merged.readings.end(),
            entry.observation.readings.begin(),
            entry.observation.readings.end());
    }
    return merged;
}

} // namespace

ObservationWindow::ObservationWindow(WindowOptions options)
    : options_(options) {}

void ObservationWindow::add(ScalarObservation observation) {
    auto time_ns = observation.sample_time_ns;
    entries_.push_back(WindowEntry{
        time_ns,
        std::move(observation),
    });
    if (options_.width_ns != 0 && time_ns >= options_.width_ns) {
        expire_before(time_ns - options_.width_ns);
    }
    enforce_limits();
}

void ObservationWindow::expire_before(std::uint64_t time_ns) {
    while (!entries_.empty() && entries_.front().time_ns < time_ns) {
        entries_.pop_front();
    }
}

void ObservationWindow::clear() {
    entries_.clear();
}

std::optional<WindowEntry> ObservationWindow::latest() const {
    if (entries_.empty()) {
        return std::nullopt;
    }
    return entries_.back();
}

WindowSnapshot ObservationWindow::snapshot() const {
    WindowSnapshot snapshot;
    if (entries_.empty()) {
        return snapshot;
    }
    snapshot.start_time_ns = entries_.front().time_ns;
    snapshot.end_time_ns = entries_.back().time_ns;
    snapshot.observations = entries_.size();
    snapshot.stats = analyze_scalar_observation(merge_observations(entries_));
    return snapshot;
}

const std::deque<WindowEntry>& ObservationWindow::entries() const noexcept {
    return entries_;
}

void ObservationWindow::enforce_limits() {
    while (entries_.size() > options_.max_observations) {
        entries_.pop_front();
    }
}

std::string render_window_snapshot(const WindowSnapshot& snapshot) {
    std::ostringstream out;
    out << "observation_window\n"
        << "  start_time_ns: "
        << snapshot.start_time_ns
        << "\n"
        << "  end_time_ns: "
        << snapshot.end_time_ns
        << "\n"
        << "  observations: "
        << snapshot.observations
        << "\n"
        << render_observation_stats(snapshot.stats);
    return out.str();
}

std::vector<ScalarObservation> extract_observations(const ObservationWindow& window) {
    std::vector<ScalarObservation> observations;
    observations.reserve(window.entries().size());
    for (const auto& entry : window.entries()) {
        observations.push_back(entry.observation);
    }
    return observations;
}

} // namespace aethon::telemetry
