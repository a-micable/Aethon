#pragma once

#include "aethon/telemetry/payload.hpp"
#include "aethon/telemetry/statistics.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace aethon::telemetry {

struct WindowOptions {
    std::uint64_t width_ns = 1'000'000'000ULL;
    std::size_t max_observations = 4096;
};

struct WindowEntry {
    std::uint64_t time_ns = 0;
    ScalarObservation observation;
};

struct WindowSnapshot {
    std::uint64_t start_time_ns = 0;
    std::uint64_t end_time_ns = 0;
    std::size_t observations = 0;
    ObservationStats stats;
};

class ObservationWindow {
public:
    explicit ObservationWindow(WindowOptions options = {});

    void add(ScalarObservation observation);
    void expire_before(std::uint64_t time_ns);
    void clear();

    [[nodiscard]] std::optional<WindowEntry> latest() const;
    [[nodiscard]] WindowSnapshot snapshot() const;
    [[nodiscard]] const std::deque<WindowEntry>& entries() const noexcept;

private:
    void enforce_limits();

    WindowOptions options_;
    std::deque<WindowEntry> entries_;
};

[[nodiscard]] std::string render_window_snapshot(const WindowSnapshot& snapshot);
[[nodiscard]] std::vector<ScalarObservation> extract_observations(const ObservationWindow& window);

} // namespace aethon::telemetry
