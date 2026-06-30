#pragma once

#include "aethon/rf/types.hpp"
#include "aethon/telemetry/anomaly.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class NoiseEstimator : std::uint8_t {
    median = 0,
    trimmed_mean = 1,
    percentile = 2,
    rolling_percentile = 3,
};

struct NoiseFloorConfig {
    NoiseEstimator estimator = NoiseEstimator::rolling_percentile;
    double percentile = 20.0;
    double trim_ratio = 0.15;
    std::size_t window_bins = 31;
    std::size_t smoothing_bins = 5;
    double max_signal_percentile = 85.0;
};

struct PeakDetectorConfig {
    double min_prominence_db = 6.0;
    double min_snr_db = 6.0;
    double merge_gap_hz = 0.0;
    double edge_drop_db = 3.0;
    std::size_t min_width_bins = 1;
    std::size_t max_peaks = 64;
    bool use_reported_noise = true;
    NoiseFloorConfig noise;
};

struct OccupancyBin {
    std::uint64_t frequency_hz = 0;
    double power_dbm = 0.0;
    double floor_dbm = 0.0;
    double margin_db = 0.0;
    bool occupied = false;
};

struct SpectrumOccupancy {
    double occupied_ratio = 0.0;
    double mean_margin_db = 0.0;
    double max_margin_db = 0.0;
    std::vector<OccupancyBin> bins;
};

struct SpectralAnalysis {
    SpectrumSlice slice;
    NoiseFloorEstimate noise;
    std::vector<SpectralPeak> peaks;
    SpectrumOccupancy occupancy;
    telemetry::SpectrumStats stats;
    telemetry::AnomalyReport anomaly_report;
};

[[nodiscard]] NoiseFloorEstimate estimate_noise_floor(const SpectrumSlice& slice,
                                                      const NoiseFloorConfig& config = {});
[[nodiscard]] std::vector<SpectralPeak> detect_spectral_peaks(const SpectrumSlice& slice,
                                                              const PeakDetectorConfig& config = {});
[[nodiscard]] SpectrumOccupancy estimate_occupancy(const SpectrumSlice& slice,
                                                   const NoiseFloorEstimate& floor,
                                                   double occupied_margin_db = 6.0);
[[nodiscard]] SpectralAnalysis analyze_spectrum(const SpectrumSlice& slice,
                                                const PeakDetectorConfig& config = {});
[[nodiscard]] SpectralAnalysis analyze_spectrum_frame(const telemetry::SpectrumFrame& frame,
                                                      const PeakDetectorConfig& config = {});
[[nodiscard]] SpectrumSlice smooth_spectrum(const SpectrumSlice& slice, std::size_t window_bins);
[[nodiscard]] SpectrumSlice subtract_noise_floor(const SpectrumSlice& slice, const NoiseFloorEstimate& floor);
[[nodiscard]] double integrated_power_dbm(const std::vector<SpectrumPoint>& points,
                                          std::size_t first,
                                          std::size_t last);
[[nodiscard]] double percentile(std::vector<double> values, double percentile);
[[nodiscard]] std::string render_noise_floor(const NoiseFloorEstimate& estimate);
[[nodiscard]] std::string render_peak(const SpectralPeak& peak);
[[nodiscard]] std::string render_spectral_analysis(const SpectralAnalysis& analysis);

} // namespace aethon::rf
