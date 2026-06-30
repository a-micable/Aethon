#pragma once

#include "aethon/telemetry/payload.hpp"
#include "aethon/telemetry/statistics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class SpectrumScale : std::uint8_t {
    dbm = 0,
    dbfs = 1,
    relative_db = 2,
};

enum class RfService : std::uint8_t {
    unknown = 0,
    amateur = 1,
    broadcast = 2,
    cellular = 3,
    navigation = 4,
    satellite = 5,
    aeronautical = 6,
    maritime = 7,
    land_mobile = 8,
    ism = 9,
    radar = 10,
    public_safety = 11,
    weather = 12,
    telemetry = 13,
};

enum class Polarization : std::uint8_t {
    unknown = 0,
    vertical = 1,
    horizontal = 2,
    circular_left = 3,
    circular_right = 4,
    mixed = 5,
};

enum class PeakShape : std::uint8_t {
    unknown = 0,
    narrowband = 1,
    wideband = 2,
    flat_top = 3,
    multi_tone = 4,
    shoulder = 5,
    impulse = 6,
};

enum class InterferenceKind : std::uint8_t {
    none = 0,
    co_channel = 1,
    adjacent_channel = 2,
    broadband_noise = 3,
    narrowband_carrier = 4,
    impulsive = 5,
    intermodulation = 6,
    overload = 7,
    unknown = 8,
};

enum class SweepIntent : std::uint8_t {
    survey = 0,
    occupancy = 1,
    interference_hunt = 2,
    calibration = 3,
    compliance = 4,
};

struct FrequencyRange {
    std::uint64_t lower_hz = 0;
    std::uint64_t upper_hz = 0;

    [[nodiscard]] bool empty() const noexcept {
        return upper_hz <= lower_hz;
    }

    [[nodiscard]] std::uint64_t width_hz() const noexcept {
        return upper_hz > lower_hz ? upper_hz - lower_hz : 0;
    }

    [[nodiscard]] std::uint64_t center_hz() const noexcept {
        return lower_hz + width_hz() / 2;
    }

    [[nodiscard]] bool contains(std::uint64_t frequency_hz) const noexcept {
        return frequency_hz >= lower_hz && frequency_hz < upper_hz;
    }

    [[nodiscard]] bool overlaps(const FrequencyRange& other) const noexcept {
        return lower_hz < other.upper_hz && other.lower_hz < upper_hz;
    }
};

struct SpectrumPoint {
    std::uint64_t frequency_hz = 0;
    double power_dbm = 0.0;
    double noise_dbm = 0.0;
};

struct SpectrumSlice {
    SpectrumScale scale = SpectrumScale::dbm;
    std::uint64_t center_frequency_hz = 0;
    std::uint64_t span_hz = 0;
    std::uint64_t bin_width_hz = 0;
    std::vector<SpectrumPoint> points;
};

struct SpectralPeak {
    std::uint64_t center_frequency_hz = 0;
    std::uint64_t lower_frequency_hz = 0;
    std::uint64_t upper_frequency_hz = 0;
    std::size_t center_bin = 0;
    std::size_t lower_bin = 0;
    std::size_t upper_bin = 0;
    double peak_power_dbm = 0.0;
    double noise_floor_dbm = 0.0;
    double prominence_db = 0.0;
    double integrated_power_dbm = 0.0;
    double occupied_bandwidth_hz = 0.0;
    PeakShape shape = PeakShape::unknown;
};

struct NoiseFloorEstimate {
    double floor_dbm = 0.0;
    double median_dbm = 0.0;
    double deviation_db = 0.0;
    double slope_db_per_mhz = 0.0;
    std::size_t samples = 0;
    std::vector<double> floor_by_bin;
};

struct IqSample {
    double i = 0.0;
    double q = 0.0;
};

struct IqWindow {
    std::uint64_t sample_rate_hz = 0;
    std::uint64_t center_frequency_hz = 0;
    std::vector<IqSample> samples;
};

struct IqSummary {
    telemetry::RunningStats i;
    telemetry::RunningStats q;
    telemetry::RunningStats magnitude;
    telemetry::RunningStats phase_radians;
    double mean_i = 0.0;
    double mean_q = 0.0;
    double rms = 0.0;
    double peak_magnitude = 0.0;
    double crest_factor_db = 0.0;
    double dc_offset_magnitude = 0.0;
    double iq_gain_imbalance_db = 0.0;
    double iq_phase_error_degrees = 0.0;
    double clipping_ratio = 0.0;
    double zero_crossing_rate = 0.0;
    double estimated_frequency_offset_hz = 0.0;
    std::size_t sample_count = 0;
};

struct SignalQualityScore {
    double score = 0.0;
    double snr_db = 0.0;
    double dynamic_range_db = 0.0;
    double occupancy_ratio = 0.0;
    double interference_penalty = 0.0;
    double stability_penalty = 0.0;
    std::vector<std::string> findings;
    std::vector<std::string> recommendations;
};

[[nodiscard]] SpectrumSlice spectrum_slice_from_frame(const telemetry::SpectrumFrame& frame);
[[nodiscard]] telemetry::SpectrumFrame spectrum_frame_from_slice(const SpectrumSlice& slice);
[[nodiscard]] std::optional<FrequencyRange> intersect(FrequencyRange left, FrequencyRange right);
[[nodiscard]] FrequencyRange merge(FrequencyRange left, FrequencyRange right);
[[nodiscard]] std::string format_frequency(std::uint64_t frequency_hz);
[[nodiscard]] std::string format_range(FrequencyRange range);
[[nodiscard]] std::string rf_service_name(RfService service);
[[nodiscard]] std::string peak_shape_name(PeakShape shape);
[[nodiscard]] std::string interference_kind_name(InterferenceKind kind);

} // namespace aethon::rf
