#pragma once

#include "aethon/rf/interference.hpp"
#include "aethon/rf/signal_quality.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace aethon::rf {

enum class TrendDirection : std::uint8_t {
    stable = 0,
    rising = 1,
    falling = 2,
    intermittent = 3,
    new_signal = 4,
    disappeared = 5,
};

struct SpectrumObservation {
    std::uint64_t time_ns = 0;
    SpectralAnalysis analysis;
    InterferenceReport interference;
    SignalQualityScore quality;
};

struct PeakTrackPoint {
    std::uint64_t time_ns = 0;
    std::uint64_t center_frequency_hz = 0;
    double peak_power_dbm = 0.0;
    double prominence_db = 0.0;
};

struct PeakTrack {
    std::uint64_t nominal_frequency_hz = 0;
    FrequencyRange observed_range;
    double persistence = 0.0;
    double mean_power_dbm = 0.0;
    double power_slope_db_per_hour = 0.0;
    double frequency_drift_hz = 0.0;
    TrendDirection trend = TrendDirection::stable;
    std::vector<PeakTrackPoint> points;
};

struct OccupancyTrend {
    double first_ratio = 0.0;
    double last_ratio = 0.0;
    double mean_ratio = 0.0;
    double slope_per_hour = 0.0;
    TrendDirection trend = TrendDirection::stable;
};

struct SpectrumHistoryReport {
    std::vector<PeakTrack> tracks;
    OccupancyTrend occupancy;
    double mean_quality_score = 0.0;
    double quality_slope_per_hour = 0.0;
    std::vector<std::string> findings;
};

class SpectrumHistory {
public:
    explicit SpectrumHistory(std::size_t max_observations = 128);

    void add(SpectrumObservation observation);
    void clear();

    [[nodiscard]] const std::vector<SpectrumObservation>& observations() const noexcept;
    [[nodiscard]] SpectrumHistoryReport analyze(std::uint64_t match_tolerance_hz = 25'000) const;
    [[nodiscard]] std::optional<SpectrumObservation> latest() const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::size_t max_observations_ = 128;
    std::vector<SpectrumObservation> observations_;
};

[[nodiscard]] SpectrumObservation make_spectrum_observation(std::uint64_t time_ns,
                                                           const telemetry::SpectrumFrame& frame,
                                                           const BandPlan& band_plan);
[[nodiscard]] std::vector<PeakTrack> build_peak_tracks(const std::vector<SpectrumObservation>& observations,
                                                       std::uint64_t match_tolerance_hz);
[[nodiscard]] OccupancyTrend analyze_occupancy_trend(const std::vector<SpectrumObservation>& observations);
[[nodiscard]] std::vector<PeakTrack> tracks_in_range(const std::vector<PeakTrack>& tracks,
                                                     FrequencyRange range);
[[nodiscard]] std::vector<PeakTrack> tracks_with_trend(const std::vector<PeakTrack>& tracks,
                                                       TrendDirection trend);
[[nodiscard]] std::vector<PeakTrack> strongest_tracks(std::vector<PeakTrack> tracks,
                                                      std::size_t limit,
                                                      double min_persistence = 0.0);
[[nodiscard]] std::vector<std::string> summarize_history_findings(const SpectrumHistoryReport& report);
[[nodiscard]] double slope_per_hour(const std::vector<std::pair<std::uint64_t, double>>& samples);
[[nodiscard]] TrendDirection classify_trend(double slope,
                                            double first,
                                            double last,
                                            double intermittent_ratio = 0.0);
[[nodiscard]] std::string trend_direction_name(TrendDirection direction);
[[nodiscard]] std::string render_peak_track(const PeakTrack& track);
[[nodiscard]] std::string render_spectrum_history_report(const SpectrumHistoryReport& report);

} // namespace aethon::rf
