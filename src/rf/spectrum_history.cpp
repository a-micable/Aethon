#include "aethon/rf/spectrum_history.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

std::uint64_t frequency_delta(std::uint64_t left, std::uint64_t right) {
    return left > right ? left - right : right - left;
}

double hours_between(std::uint64_t first_ns, std::uint64_t last_ns) {
    if (last_ns <= first_ns) {
        return 0.0;
    }
    return static_cast<double>(last_ns - first_ns) / 3'600'000'000'000.0;
}

double mean_power(const std::vector<PeakTrackPoint>& points) {
    if (points.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& point : points) {
        total += point.peak_power_dbm;
    }
    return total / static_cast<double>(points.size());
}

std::uint64_t nominal_frequency(const std::vector<PeakTrackPoint>& points) {
    if (points.empty()) {
        return 0;
    }
    std::vector<std::uint64_t> values;
    values.reserve(points.size());
    for (const auto& point : points) {
        values.push_back(point.center_frequency_hz);
    }
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

FrequencyRange observed_range(const std::vector<PeakTrackPoint>& points) {
    FrequencyRange range;
    for (const auto& point : points) {
        FrequencyRange single{point.center_frequency_hz, point.center_frequency_hz + 1};
        range = merge(range, single);
    }
    return range;
}

std::vector<std::pair<std::uint64_t, double>> power_samples(const std::vector<PeakTrackPoint>& points) {
    std::vector<std::pair<std::uint64_t, double>> samples;
    samples.reserve(points.size());
    for (const auto& point : points) {
        samples.push_back({point.time_ns, point.peak_power_dbm});
    }
    return samples;
}

double frequency_drift(const std::vector<PeakTrackPoint>& points) {
    if (points.size() < 2) {
        return 0.0;
    }
    auto minmax = std::minmax_element(points.begin(), points.end(), [](const auto& left, const auto& right) {
        return left.center_frequency_hz < right.center_frequency_hz;
    });
    return static_cast<double>(frequency_delta(minmax.first->center_frequency_hz, minmax.second->center_frequency_hz));
}

void finalize_track(PeakTrack& track, std::size_t observations) {
    track.nominal_frequency_hz = nominal_frequency(track.points);
    track.observed_range = observed_range(track.points);
    track.mean_power_dbm = mean_power(track.points);
    track.power_slope_db_per_hour = slope_per_hour(power_samples(track.points));
    track.frequency_drift_hz = frequency_drift(track.points);
    track.persistence = observations == 0 ? 0.0 : static_cast<double>(track.points.size()) / static_cast<double>(observations);
    auto first = track.points.empty() ? 0.0 : track.points.front().peak_power_dbm;
    auto last = track.points.empty() ? 0.0 : track.points.back().peak_power_dbm;
    auto missing_ratio = 1.0 - track.persistence;
    track.trend = classify_trend(track.power_slope_db_per_hour, first, last, missing_ratio);
}

std::optional<std::size_t> best_track_for_peak(const std::vector<PeakTrack>& tracks,
                                               const SpectralPeak& peak,
                                               std::uint64_t tolerance_hz) {
    std::optional<std::size_t> best;
    auto best_delta = tolerance_hz + 1;
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        auto reference = tracks[i].nominal_frequency_hz;
        if (reference == 0 && !tracks[i].points.empty()) {
            reference = tracks[i].points.back().center_frequency_hz;
        }
        auto delta = frequency_delta(reference, peak.center_frequency_hz);
        if (delta <= tolerance_hz && delta < best_delta) {
            best = i;
            best_delta = delta;
        }
    }
    return best;
}

void add_history_finding(SpectrumHistoryReport& report, std::string finding) {
    report.findings.push_back(std::move(finding));
}

} // namespace

SpectrumHistory::SpectrumHistory(std::size_t max_observations)
    : max_observations_(std::max<std::size_t>(1, max_observations)) {}

void SpectrumHistory::add(SpectrumObservation observation) {
    observations_.push_back(std::move(observation));
    std::sort(observations_.begin(), observations_.end(), [](const auto& left, const auto& right) {
        return left.time_ns < right.time_ns;
    });
    while (observations_.size() > max_observations_) {
        observations_.erase(observations_.begin());
    }
}

void SpectrumHistory::clear() {
    observations_.clear();
}

const std::vector<SpectrumObservation>& SpectrumHistory::observations() const noexcept {
    return observations_;
}

SpectrumHistoryReport SpectrumHistory::analyze(std::uint64_t match_tolerance_hz) const {
    SpectrumHistoryReport report;
    if (observations_.empty()) {
        add_history_finding(report, "no spectrum observations available");
        return report;
    }
    report.tracks = build_peak_tracks(observations_, match_tolerance_hz);
    report.occupancy = analyze_occupancy_trend(observations_);

    std::vector<std::pair<std::uint64_t, double>> quality_samples;
    quality_samples.reserve(observations_.size());
    double total_quality = 0.0;
    for (const auto& observation : observations_) {
        quality_samples.push_back({observation.time_ns, observation.quality.score});
        total_quality += observation.quality.score;
    }
    report.mean_quality_score = total_quality / static_cast<double>(observations_.size());
    report.quality_slope_per_hour = slope_per_hour(quality_samples);

    for (const auto& track : report.tracks) {
        if (track.trend == TrendDirection::new_signal) {
            add_history_finding(report, "new persistent peak near " + format_frequency(track.nominal_frequency_hz));
        }
        if (track.trend == TrendDirection::rising && track.power_slope_db_per_hour > 6.0) {
            add_history_finding(report, "rapidly rising peak near " + format_frequency(track.nominal_frequency_hz));
        }
        if (track.frequency_drift_hz > static_cast<double>(match_tolerance_hz)) {
            add_history_finding(report, "frequency drift exceeds match tolerance near " + format_frequency(track.nominal_frequency_hz));
        }
    }
    if (report.occupancy.trend == TrendDirection::rising) {
        add_history_finding(report, "spectrum occupancy is rising");
    }
    if (report.quality_slope_per_hour < -5.0) {
        add_history_finding(report, "quality score is degrading over time");
    }
    return report;
}

std::optional<SpectrumObservation> SpectrumHistory::latest() const {
    if (observations_.empty()) {
        return std::nullopt;
    }
    return observations_.back();
}

std::size_t SpectrumHistory::size() const noexcept {
    return observations_.size();
}

SpectrumObservation make_spectrum_observation(std::uint64_t time_ns,
                                             const telemetry::SpectrumFrame& frame,
                                             const BandPlan& band_plan) {
    SpectrumObservation observation;
    observation.time_ns = time_ns;
    observation.analysis = rf::analyze_spectrum_frame(frame);
    observation.interference = classify_interference(observation.analysis, band_plan);
    SignalQualityInput input;
    input.spectral = observation.analysis;
    input.interference = observation.interference;
    input.allocation = band_plan.best_allocation(observation.analysis.slice.center_frequency_hz);
    observation.quality = score_signal_quality(input);
    return observation;
}

std::vector<PeakTrack> build_peak_tracks(const std::vector<SpectrumObservation>& observations,
                                         std::uint64_t match_tolerance_hz) {
    std::vector<PeakTrack> tracks;
    for (const auto& observation : observations) {
        for (const auto& peak : observation.analysis.peaks) {
            auto match = best_track_for_peak(tracks, peak, match_tolerance_hz);
            PeakTrackPoint point;
            point.time_ns = observation.time_ns;
            point.center_frequency_hz = peak.center_frequency_hz;
            point.peak_power_dbm = peak.peak_power_dbm;
            point.prominence_db = peak.prominence_db;
            if (match) {
                tracks[*match].points.push_back(point);
                tracks[*match].nominal_frequency_hz = nominal_frequency(tracks[*match].points);
            } else {
                PeakTrack track;
                track.nominal_frequency_hz = peak.center_frequency_hz;
                track.points.push_back(point);
                tracks.push_back(track);
            }
        }
    }
    for (auto& track : tracks) {
        std::sort(track.points.begin(), track.points.end(), [](const auto& left, const auto& right) {
            return left.time_ns < right.time_ns;
        });
        finalize_track(track, observations.size());
    }
    std::sort(tracks.begin(), tracks.end(), [](const auto& left, const auto& right) {
        if (left.persistence != right.persistence) {
            return left.persistence > right.persistence;
        }
        return left.mean_power_dbm > right.mean_power_dbm;
    });
    return tracks;
}

std::vector<PeakTrack> tracks_in_range(const std::vector<PeakTrack>& tracks, FrequencyRange range) {
    std::vector<PeakTrack> matches;
    for (const auto& track : tracks) {
        if (track.observed_range.overlaps(range) || range.contains(track.nominal_frequency_hz)) {
            matches.push_back(track);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
        if (left.persistence != right.persistence) {
            return left.persistence > right.persistence;
        }
        return left.nominal_frequency_hz < right.nominal_frequency_hz;
    });
    return matches;
}

std::vector<PeakTrack> tracks_with_trend(const std::vector<PeakTrack>& tracks, TrendDirection trend) {
    std::vector<PeakTrack> matches;
    for (const auto& track : tracks) {
        if (track.trend == trend) {
            matches.push_back(track);
        }
    }
    std::sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
        if (std::abs(left.power_slope_db_per_hour) != std::abs(right.power_slope_db_per_hour)) {
            return std::abs(left.power_slope_db_per_hour) > std::abs(right.power_slope_db_per_hour);
        }
        return left.mean_power_dbm > right.mean_power_dbm;
    });
    return matches;
}

std::vector<PeakTrack> strongest_tracks(std::vector<PeakTrack> tracks,
                                        std::size_t limit,
                                        double min_persistence) {
    tracks.erase(
        std::remove_if(tracks.begin(), tracks.end(), [&](const auto& track) {
            return track.persistence < min_persistence;
        }),
        tracks.end());
    std::sort(tracks.begin(), tracks.end(), [](const auto& left, const auto& right) {
        if (left.mean_power_dbm != right.mean_power_dbm) {
            return left.mean_power_dbm > right.mean_power_dbm;
        }
        return left.persistence > right.persistence;
    });
    if (tracks.size() > limit) {
        tracks.resize(limit);
    }
    return tracks;
}

std::vector<std::string> summarize_history_findings(const SpectrumHistoryReport& report) {
    std::vector<std::string> summaries = report.findings;
    auto persistent = strongest_tracks(report.tracks, 3, 0.75);
    for (const auto& track : persistent) {
        summaries.push_back(
            "persistent carrier near "
            + format_frequency(track.nominal_frequency_hz)
            + " at "
            + std::to_string(track.mean_power_dbm)
            + " dBm mean");
    }
    auto rising = tracks_with_trend(report.tracks, TrendDirection::rising);
    for (const auto& track : rising) {
        summaries.push_back(
            "rising carrier near "
            + format_frequency(track.nominal_frequency_hz)
            + " slope "
            + std::to_string(track.power_slope_db_per_hour)
            + " dB/hour");
    }
    if (report.occupancy.trend != TrendDirection::stable) {
        summaries.push_back(
            "occupancy is "
            + trend_direction_name(report.occupancy.trend)
            + " from "
            + std::to_string(report.occupancy.first_ratio)
            + " to "
            + std::to_string(report.occupancy.last_ratio));
    }
    return summaries;
}

OccupancyTrend analyze_occupancy_trend(const std::vector<SpectrumObservation>& observations) {
    OccupancyTrend trend;
    if (observations.empty()) {
        return trend;
    }
    std::vector<std::pair<std::uint64_t, double>> samples;
    samples.reserve(observations.size());
    double total = 0.0;
    for (const auto& observation : observations) {
        auto ratio = observation.analysis.occupancy.occupied_ratio;
        samples.push_back({observation.time_ns, ratio});
        total += ratio;
    }
    trend.first_ratio = samples.front().second;
    trend.last_ratio = samples.back().second;
    trend.mean_ratio = total / static_cast<double>(samples.size());
    trend.slope_per_hour = slope_per_hour(samples);
    trend.trend = classify_trend(trend.slope_per_hour, trend.first_ratio, trend.last_ratio);
    return trend;
}

double slope_per_hour(const std::vector<std::pair<std::uint64_t, double>>& samples) {
    if (samples.size() < 2) {
        return 0.0;
    }
    auto first_time = samples.front().first;
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;
    double sum_xy = 0.0;
    for (const auto& sample : samples) {
        auto x = hours_between(first_time, sample.first);
        auto y = sample.second;
        sum_x += x;
        sum_y += y;
        sum_xx += x * x;
        sum_xy += x * y;
    }
    auto n = static_cast<double>(samples.size());
    auto denominator = n * sum_xx - sum_x * sum_x;
    if (std::abs(denominator) < 1.0e-12) {
        return 0.0;
    }
    return (n * sum_xy - sum_x * sum_y) / denominator;
}

TrendDirection classify_trend(double slope,
                              double first,
                              double last,
                              double intermittent_ratio) {
    auto change = last - first;
    if (intermittent_ratio > 0.55) {
        return TrendDirection::intermittent;
    }
    if (first == 0.0 && last != 0.0) {
        return TrendDirection::new_signal;
    }
    if (first != 0.0 && last == 0.0) {
        return TrendDirection::disappeared;
    }
    if (slope > 1.0 || change > 3.0) {
        return TrendDirection::rising;
    }
    if (slope < -1.0 || change < -3.0) {
        return TrendDirection::falling;
    }
    return TrendDirection::stable;
}

std::string trend_direction_name(TrendDirection direction) {
    switch (direction) {
    case TrendDirection::stable:
        return "stable";
    case TrendDirection::rising:
        return "rising";
    case TrendDirection::falling:
        return "falling";
    case TrendDirection::intermittent:
        return "intermittent";
    case TrendDirection::new_signal:
        return "new_signal";
    case TrendDirection::disappeared:
        return "disappeared";
    }
    return "stable";
}

std::string render_peak_track(const PeakTrack& track) {
    std::ostringstream out;
    out << "peak_track frequency="
        << format_frequency(track.nominal_frequency_hz)
        << " range="
        << format_range(track.observed_range)
        << " persistence="
        << track.persistence
        << " mean_power_dbm="
        << track.mean_power_dbm
        << " slope_db_per_hour="
        << track.power_slope_db_per_hour
        << " drift_hz="
        << track.frequency_drift_hz
        << " trend="
        << trend_direction_name(track.trend)
        << " points="
        << track.points.size();
    return out.str();
}

std::string render_spectrum_history_report(const SpectrumHistoryReport& report) {
    std::ostringstream out;
    out << "spectrum_history\n"
        << "  tracks: "
        << report.tracks.size()
        << "\n"
        << "  occupancy first="
        << report.occupancy.first_ratio
        << " last="
        << report.occupancy.last_ratio
        << " mean="
        << report.occupancy.mean_ratio
        << " slope_per_hour="
        << report.occupancy.slope_per_hour
        << " trend="
        << trend_direction_name(report.occupancy.trend)
        << "\n"
        << "  mean_quality_score: "
        << report.mean_quality_score
        << "\n"
        << "  quality_slope_per_hour: "
        << report.quality_slope_per_hour
        << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: " << finding << "\n";
    }
    for (const auto& track : report.tracks) {
        out << "  " << render_peak_track(track) << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
