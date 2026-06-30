#pragma once

#include "aethon/telemetry/payload.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::telemetry {

struct CalibrationPoint {
    double raw = 0.0;
    double corrected = 0.0;
};

struct ChannelCalibration {
    std::uint16_t channel = 0;
    double gain = 1.0;
    double offset = 0.0;
    std::uint64_t valid_from_ns = 0;
    std::uint64_t valid_until_ns = 0;
    std::vector<CalibrationPoint> curve;
};

struct CalibrationTable {
    std::string name;
    std::vector<ChannelCalibration> channels;
};

struct CalibrationResult {
    ScalarObservation observation;
    std::vector<std::string> warnings;
};

[[nodiscard]] std::optional<ChannelCalibration> find_calibration(const CalibrationTable& table,
                                                                 std::uint16_t channel,
                                                                 std::uint64_t time_ns);

[[nodiscard]] double apply_calibration(double raw, const ChannelCalibration& calibration);
[[nodiscard]] CalibrationResult calibrate_observation(const ScalarObservation& observation,
                                                      const CalibrationTable& table);

[[nodiscard]] std::string render_calibration_table(const CalibrationTable& table);
[[nodiscard]] std::string render_calibration_warnings(const CalibrationResult& result);

} // namespace aethon::telemetry
