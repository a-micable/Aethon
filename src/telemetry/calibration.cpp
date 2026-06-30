#include "aethon/telemetry/calibration.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace aethon::telemetry {
namespace {

bool valid_at(const ChannelCalibration& calibration, std::uint64_t time_ns) {
    if (time_ns < calibration.valid_from_ns) {
        return false;
    }
    if (calibration.valid_until_ns != 0 && time_ns >= calibration.valid_until_ns) {
        return false;
    }
    return true;
}

double interpolate(double raw, const CalibrationPoint& left, const CalibrationPoint& right) {
    auto span = right.raw - left.raw;
    if (std::abs(span) < 0.000001) {
        return left.corrected;
    }
    auto position = (raw - left.raw) / span;
    return left.corrected + position * (right.corrected - left.corrected);
}

double apply_curve(double raw, const std::vector<CalibrationPoint>& curve) {
    if (curve.empty()) {
        return raw;
    }
    if (curve.size() == 1) {
        return curve.front().corrected;
    }
    if (raw <= curve.front().raw) {
        return interpolate(raw, curve[0], curve[1]);
    }
    for (std::size_t i = 1; i < curve.size(); ++i) {
        if (raw <= curve[i].raw) {
            return interpolate(raw, curve[i - 1], curve[i]);
        }
    }
    return interpolate(raw, curve[curve.size() - 2], curve.back());
}

std::vector<CalibrationPoint> sorted_curve(std::vector<CalibrationPoint> curve) {
    std::sort(
        curve.begin(),
        curve.end(),
        [](const CalibrationPoint& left, const CalibrationPoint& right) {
            return left.raw < right.raw;
        });
    return curve;
}

} // namespace

std::optional<ChannelCalibration> find_calibration(const CalibrationTable& table,
                                                   std::uint16_t channel,
                                                   std::uint64_t time_ns) {
    for (const auto& calibration : table.channels) {
        if (calibration.channel == channel && valid_at(calibration, time_ns)) {
            return calibration;
        }
    }
    return std::nullopt;
}

double apply_calibration(double raw, const ChannelCalibration& calibration) {
    if (!calibration.curve.empty()) {
        auto curve = sorted_curve(calibration.curve);
        return apply_curve(raw, curve);
    }
    return raw * calibration.gain + calibration.offset;
}

CalibrationResult calibrate_observation(const ScalarObservation& observation,
                                        const CalibrationTable& table) {
    CalibrationResult result;
    result.observation = observation;
    for (auto& reading : result.observation.readings) {
        auto calibration = find_calibration(table, reading.channel, observation.sample_time_ns);
        if (!calibration) {
            reading.quality = ReadingQuality::missing_calibration;
            std::ostringstream warning;
            warning << "missing calibration for channel " << reading.channel;
            result.warnings.push_back(warning.str());
            continue;
        }
        auto corrected = apply_calibration(static_cast<double>(reading.value), *calibration);
        reading.value = static_cast<std::int32_t>(std::llround(corrected));
    }
    return result;
}

std::string render_calibration_table(const CalibrationTable& table) {
    std::ostringstream out;
    out << "calibration_table "
        << table.name
        << "\n"
        << "  channels: "
        << table.channels.size()
        << "\n";
    for (const auto& channel : table.channels) {
        out << "  channel: "
            << channel.channel
            << " gain="
            << channel.gain
            << " offset="
            << channel.offset
            << " valid_from_ns="
            << channel.valid_from_ns
            << " valid_until_ns="
            << channel.valid_until_ns
            << " curve_points="
            << channel.curve.size()
            << "\n";
    }
    return out.str();
}

std::string render_calibration_warnings(const CalibrationResult& result) {
    std::ostringstream out;
    if (result.warnings.empty()) {
        out << "calibration warnings: none\n";
        return out.str();
    }
    out << "calibration warnings:\n";
    for (const auto& warning : result.warnings) {
        out << "  "
            << warning
            << "\n";
    }
    return out.str();
}

} // namespace aethon::telemetry
