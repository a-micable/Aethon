#include "aethon/rf/types.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace aethon::rf {
namespace {

double clamp_dbm(double value) {
    if (value > static_cast<double>(std::numeric_limits<std::int16_t>::max()) / 10.0) {
        return static_cast<double>(std::numeric_limits<std::int16_t>::max()) / 10.0;
    }
    if (value < static_cast<double>(std::numeric_limits<std::int16_t>::min()) / 10.0) {
        return static_cast<double>(std::numeric_limits<std::int16_t>::min()) / 10.0;
    }
    return value;
}

std::uint64_t bin_frequency(const telemetry::SpectrumFrame& frame, std::size_t index) {
    if (frame.bins.empty()) {
        return frame.center_frequency_hz;
    }
    const auto total_width = static_cast<std::uint64_t>(frame.bin_width_hz) * frame.bins.size();
    const auto lower = frame.center_frequency_hz >= total_width / 2
        ? frame.center_frequency_hz - total_width / 2
        : 0;
    return lower + static_cast<std::uint64_t>(index) * frame.bin_width_hz + frame.bin_width_hz / 2;
}

std::string format_scaled(std::uint64_t value, double divisor, const char* suffix, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << static_cast<double>(value) / divisor << suffix;
    return out.str();
}

} // namespace

SpectrumSlice spectrum_slice_from_frame(const telemetry::SpectrumFrame& frame) {
    SpectrumSlice slice;
    slice.center_frequency_hz = frame.center_frequency_hz;
    slice.span_hz = frame.span_hz;
    slice.bin_width_hz = frame.bin_width_hz;
    slice.points.reserve(frame.bins.size());
    for (std::size_t i = 0; i < frame.bins.size(); ++i) {
        SpectrumPoint point;
        point.frequency_hz = bin_frequency(frame, i);
        point.power_dbm = static_cast<double>(frame.bins[i].power_dbm_x10) / 10.0;
        point.noise_dbm = static_cast<double>(frame.bins[i].noise_dbm_x10) / 10.0;
        slice.points.push_back(point);
    }
    return slice;
}

telemetry::SpectrumFrame spectrum_frame_from_slice(const SpectrumSlice& slice) {
    telemetry::SpectrumFrame frame;
    frame.center_frequency_hz = slice.center_frequency_hz;
    frame.span_hz = static_cast<std::uint32_t>(std::min<std::uint64_t>(slice.span_hz, std::numeric_limits<std::uint32_t>::max()));
    frame.bin_width_hz = static_cast<std::uint16_t>(std::min<std::uint64_t>(slice.bin_width_hz, std::numeric_limits<std::uint16_t>::max()));
    frame.bins.reserve(slice.points.size());
    for (const auto& point : slice.points) {
        telemetry::SpectrumBin bin;
        bin.power_dbm_x10 = static_cast<std::int16_t>(std::llround(clamp_dbm(point.power_dbm) * 10.0));
        bin.noise_dbm_x10 = static_cast<std::int16_t>(std::llround(clamp_dbm(point.noise_dbm) * 10.0));
        frame.bins.push_back(bin);
    }
    return frame;
}

std::optional<FrequencyRange> intersect(FrequencyRange left, FrequencyRange right) {
    FrequencyRange result;
    result.lower_hz = std::max(left.lower_hz, right.lower_hz);
    result.upper_hz = std::min(left.upper_hz, right.upper_hz);
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
}

FrequencyRange merge(FrequencyRange left, FrequencyRange right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    return {std::min(left.lower_hz, right.lower_hz), std::max(left.upper_hz, right.upper_hz)};
}

std::string format_frequency(std::uint64_t frequency_hz) {
    if (frequency_hz >= 1'000'000'000ULL) {
        return format_scaled(frequency_hz, 1'000'000'000.0, " GHz", 6);
    }
    if (frequency_hz >= 1'000'000ULL) {
        return format_scaled(frequency_hz, 1'000'000.0, " MHz", 3);
    }
    if (frequency_hz >= 1'000ULL) {
        return format_scaled(frequency_hz, 1'000.0, " kHz", 3);
    }
    std::ostringstream out;
    out << frequency_hz << " Hz";
    return out.str();
}

std::string format_range(FrequencyRange range) {
    std::ostringstream out;
    out << format_frequency(range.lower_hz) << " - " << format_frequency(range.upper_hz);
    return out.str();
}

std::string rf_service_name(RfService service) {
    switch (service) {
    case RfService::unknown:
        return "unknown";
    case RfService::amateur:
        return "amateur";
    case RfService::broadcast:
        return "broadcast";
    case RfService::cellular:
        return "cellular";
    case RfService::navigation:
        return "navigation";
    case RfService::satellite:
        return "satellite";
    case RfService::aeronautical:
        return "aeronautical";
    case RfService::maritime:
        return "maritime";
    case RfService::land_mobile:
        return "land_mobile";
    case RfService::ism:
        return "ism";
    case RfService::radar:
        return "radar";
    case RfService::public_safety:
        return "public_safety";
    case RfService::weather:
        return "weather";
    case RfService::telemetry:
        return "telemetry";
    }
    return "unknown";
}

std::string peak_shape_name(PeakShape shape) {
    switch (shape) {
    case PeakShape::unknown:
        return "unknown";
    case PeakShape::narrowband:
        return "narrowband";
    case PeakShape::wideband:
        return "wideband";
    case PeakShape::flat_top:
        return "flat_top";
    case PeakShape::multi_tone:
        return "multi_tone";
    case PeakShape::shoulder:
        return "shoulder";
    case PeakShape::impulse:
        return "impulse";
    }
    return "unknown";
}

std::string interference_kind_name(InterferenceKind kind) {
    switch (kind) {
    case InterferenceKind::none:
        return "none";
    case InterferenceKind::co_channel:
        return "co_channel";
    case InterferenceKind::adjacent_channel:
        return "adjacent_channel";
    case InterferenceKind::broadband_noise:
        return "broadband_noise";
    case InterferenceKind::narrowband_carrier:
        return "narrowband_carrier";
    case InterferenceKind::impulsive:
        return "impulsive";
    case InterferenceKind::intermodulation:
        return "intermodulation";
    case InterferenceKind::overload:
        return "overload";
    case InterferenceKind::unknown:
        return "unknown";
    }
    return "unknown";
}

} // namespace aethon::rf
