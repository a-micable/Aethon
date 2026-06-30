#pragma once

#include "aethon/telemetry/payload.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::telemetry {

struct RunningStats {
    std::size_t count = 0;
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double variance = 0.0;
};

struct ChannelStats {
    std::uint16_t channel = 0;
    RunningStats values;
    std::size_t degraded_readings = 0;
};

struct SpectrumStats {
    RunningStats power_dbm;
    RunningStats noise_dbm;
    std::optional<std::size_t> strongest_bin;
    std::optional<std::size_t> weakest_bin;
};

struct ObservationStats {
    std::vector<ChannelStats> channels;
    std::size_t total_readings = 0;
    std::size_t degraded_readings = 0;
};

void add_sample(RunningStats& stats, double value);
[[nodiscard]] double standard_deviation(const RunningStats& stats);

[[nodiscard]] ObservationStats analyze_scalar_observation(const ScalarObservation& observation);
[[nodiscard]] SpectrumStats analyze_spectrum_frame(const SpectrumFrame& frame);

[[nodiscard]] std::string render_running_stats(const RunningStats& stats);
[[nodiscard]] std::string render_observation_stats(const ObservationStats& stats);
[[nodiscard]] std::string render_spectrum_stats(const SpectrumStats& stats);

} // namespace aethon::telemetry
