#include "aethon/rf/spectral_analysis.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

constexpr double kTinyMilliwatt = 1.0e-18;

double dbm_to_mw(double dbm) {
    return std::pow(10.0, dbm / 10.0);
}

double mw_to_dbm(double mw) {
    if (mw <= kTinyMilliwatt) {
        return -180.0;
    }
    return 10.0 * std::log10(mw);
}

double clamp_percentile(double p) {
    return std::clamp(p, 0.0, 100.0);
}

std::vector<double> powers(const SpectrumSlice& slice) {
    std::vector<double> values;
    values.reserve(slice.points.size());
    for (const auto& point : slice.points) {
        values.push_back(point.power_dbm);
    }
    return values;
}

std::vector<double> noise_values(const SpectrumSlice& slice) {
    std::vector<double> values;
    values.reserve(slice.points.size());
    for (const auto& point : slice.points) {
        values.push_back(point.noise_dbm);
    }
    return values;
}

double mean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    auto total = std::accumulate(values.begin(), values.end(), 0.0);
    return total / static_cast<double>(values.size());
}

double standard_deviation(const std::vector<double>& values, double center) {
    if (values.size() < 2) {
        return 0.0;
    }
    double total = 0.0;
    for (auto value : values) {
        auto delta = value - center;
        total += delta * delta;
    }
    return std::sqrt(total / static_cast<double>(values.size() - 1));
}

double trimmed_mean(std::vector<double> values, double trim_ratio) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    auto trim = static_cast<std::size_t>(std::floor(static_cast<double>(values.size()) * std::clamp(trim_ratio, 0.0, 0.45)));
    if (trim * 2 >= values.size()) {
        return mean(values);
    }
    auto first = values.begin() + static_cast<std::ptrdiff_t>(trim);
    auto last = values.end() - static_cast<std::ptrdiff_t>(trim);
    return std::accumulate(first, last, 0.0) / static_cast<double>(std::distance(first, last));
}

std::vector<double> rolling_percentile_floor(const SpectrumSlice& slice, const NoiseFloorConfig& config) {
    std::vector<double> floor(slice.points.size(), 0.0);
    if (slice.points.empty()) {
        return floor;
    }
    auto radius = std::max<std::size_t>(1, config.window_bins / 2);
    for (std::size_t i = 0; i < slice.points.size(); ++i) {
        auto first = i > radius ? i - radius : 0;
        auto last = std::min(slice.points.size() - 1, i + radius);
        std::vector<double> local;
        local.reserve(last - first + 1);
        for (auto j = first; j <= last; ++j) {
            local.push_back(slice.points[j].power_dbm);
        }
        floor[i] = percentile(std::move(local), config.percentile);
    }
    return floor;
}

std::vector<double> constant_floor(const SpectrumSlice& slice, double value) {
    return std::vector<double>(slice.points.size(), value);
}

std::vector<double> smooth_values(std::vector<double> values, std::size_t window_bins) {
    if (values.empty() || window_bins <= 1) {
        return values;
    }
    if (window_bins % 2 == 0) {
        ++window_bins;
    }
    auto radius = window_bins / 2;
    std::vector<double> smoothed(values.size(), 0.0);
    for (std::size_t i = 0; i < values.size(); ++i) {
        auto first = i > radius ? i - radius : 0;
        auto last = std::min(values.size() - 1, i + radius);
        double total_mw = 0.0;
        for (auto j = first; j <= last; ++j) {
            total_mw += dbm_to_mw(values[j]);
        }
        smoothed[i] = mw_to_dbm(total_mw / static_cast<double>(last - first + 1));
    }
    return smoothed;
}

double slope_db_per_mhz(const SpectrumSlice& slice, const std::vector<double>& floor) {
    if (slice.points.size() < 2 || floor.size() != slice.points.size()) {
        return 0.0;
    }
    const auto first_freq = static_cast<double>(slice.points.front().frequency_hz) / 1'000'000.0;
    const auto last_freq = static_cast<double>(slice.points.back().frequency_hz) / 1'000'000.0;
    auto span = last_freq - first_freq;
    if (std::abs(span) < 1.0e-9) {
        return 0.0;
    }
    return (floor.back() - floor.front()) / span;
}

bool local_maximum(const SpectrumSlice& slice, std::size_t index) {
    if (slice.points.empty() || index >= slice.points.size()) {
        return false;
    }
    auto value = slice.points[index].power_dbm;
    auto left = index == 0 ? -std::numeric_limits<double>::infinity() : slice.points[index - 1].power_dbm;
    auto right = index + 1 >= slice.points.size() ? -std::numeric_limits<double>::infinity() : slice.points[index + 1].power_dbm;
    return value >= left && value >= right && (value > left || value > right);
}

std::size_t expand_left(const SpectrumSlice& slice,
                        const std::vector<double>& floor,
                        std::size_t center,
                        const PeakDetectorConfig& config) {
    auto threshold = std::max(floor[center] + config.min_snr_db, slice.points[center].power_dbm - config.edge_drop_db);
    auto current = center;
    while (current > 0) {
        auto next = current - 1;
        if (slice.points[next].power_dbm < threshold && slice.points[next].power_dbm < floor[next] + config.min_snr_db) {
            break;
        }
        current = next;
    }
    return current;
}

std::size_t expand_right(const SpectrumSlice& slice,
                         const std::vector<double>& floor,
                         std::size_t center,
                         const PeakDetectorConfig& config) {
    auto threshold = std::max(floor[center] + config.min_snr_db, slice.points[center].power_dbm - config.edge_drop_db);
    auto current = center;
    while (current + 1 < slice.points.size()) {
        auto next = current + 1;
        if (slice.points[next].power_dbm < threshold && slice.points[next].power_dbm < floor[next] + config.min_snr_db) {
            break;
        }
        current = next;
    }
    return current;
}

PeakShape classify_peak_shape(const SpectrumSlice& slice,
                              const SpectralPeak& peak,
                              const std::vector<double>& floor) {
    auto width_bins = peak.upper_bin >= peak.lower_bin ? peak.upper_bin - peak.lower_bin + 1 : 0;
    if (width_bins <= 2) {
        return PeakShape::narrowband;
    }
    if (width_bins >= 12) {
        auto edge_left = slice.points[peak.lower_bin].power_dbm;
        auto edge_right = slice.points[peak.upper_bin].power_dbm;
        auto edge_mean = (edge_left + edge_right) / 2.0;
        if (peak.peak_power_dbm - edge_mean < 3.0) {
            return PeakShape::flat_top;
        }
        return PeakShape::wideband;
    }
    std::size_t local_count = 0;
    for (auto i = peak.lower_bin; i <= peak.upper_bin; ++i) {
        if (local_maximum(slice, i) && slice.points[i].power_dbm > floor[i] + 3.0) {
            ++local_count;
        }
    }
    if (local_count >= 3) {
        return PeakShape::multi_tone;
    }
    auto left_prominence = slice.points[peak.center_bin].power_dbm - slice.points[peak.lower_bin].power_dbm;
    auto right_prominence = slice.points[peak.center_bin].power_dbm - slice.points[peak.upper_bin].power_dbm;
    if (std::abs(left_prominence - right_prominence) > 6.0) {
        return PeakShape::shoulder;
    }
    return PeakShape::narrowband;
}

SpectralPeak make_peak(const SpectrumSlice& slice,
                       const std::vector<double>& floor,
                       std::size_t center,
                       const PeakDetectorConfig& config) {
    SpectralPeak peak;
    peak.center_bin = center;
    peak.lower_bin = expand_left(slice, floor, center, config);
    peak.upper_bin = expand_right(slice, floor, center, config);
    peak.center_frequency_hz = slice.points[center].frequency_hz;
    peak.lower_frequency_hz = slice.points[peak.lower_bin].frequency_hz;
    peak.upper_frequency_hz = slice.points[peak.upper_bin].frequency_hz;
    peak.peak_power_dbm = slice.points[center].power_dbm;
    peak.noise_floor_dbm = floor[center];
    peak.prominence_db = peak.peak_power_dbm - peak.noise_floor_dbm;
    peak.integrated_power_dbm = integrated_power_dbm(slice.points, peak.lower_bin, peak.upper_bin);
    peak.occupied_bandwidth_hz = peak.upper_frequency_hz >= peak.lower_frequency_hz
        ? static_cast<double>(peak.upper_frequency_hz - peak.lower_frequency_hz + slice.bin_width_hz)
        : 0.0;
    peak.shape = classify_peak_shape(slice, peak, floor);
    return peak;
}

std::vector<SpectralPeak> merge_close_peaks(std::vector<SpectralPeak> peaks, double merge_gap_hz) {
    if (peaks.empty() || merge_gap_hz <= 0.0) {
        return peaks;
    }
    std::sort(peaks.begin(), peaks.end(), [](const auto& left, const auto& right) {
        return left.lower_frequency_hz < right.lower_frequency_hz;
    });
    std::vector<SpectralPeak> merged;
    merged.push_back(peaks.front());
    for (std::size_t i = 1; i < peaks.size(); ++i) {
        auto& last = merged.back();
        const auto& current = peaks[i];
        auto gap = current.lower_frequency_hz > last.upper_frequency_hz
            ? current.lower_frequency_hz - last.upper_frequency_hz
            : 0;
        if (static_cast<double>(gap) > merge_gap_hz) {
            merged.push_back(current);
            continue;
        }
        if (current.peak_power_dbm > last.peak_power_dbm) {
            last.center_bin = current.center_bin;
            last.center_frequency_hz = current.center_frequency_hz;
            last.peak_power_dbm = current.peak_power_dbm;
            last.noise_floor_dbm = current.noise_floor_dbm;
            last.prominence_db = current.prominence_db;
        }
        last.upper_bin = std::max(last.upper_bin, current.upper_bin);
        last.upper_frequency_hz = std::max(last.upper_frequency_hz, current.upper_frequency_hz);
        last.integrated_power_dbm = mw_to_dbm(dbm_to_mw(last.integrated_power_dbm) + dbm_to_mw(current.integrated_power_dbm));
        last.occupied_bandwidth_hz = static_cast<double>(last.upper_frequency_hz - last.lower_frequency_hz);
        last.shape = PeakShape::wideband;
    }
    return merged;
}

} // namespace

NoiseFloorEstimate estimate_noise_floor(const SpectrumSlice& slice, const NoiseFloorConfig& config) {
    NoiseFloorEstimate estimate;
    estimate.samples = slice.points.size();
    if (slice.points.empty()) {
        return estimate;
    }

    auto values = powers(slice);
    auto reported_noise = noise_values(slice);
    auto reported_mean = mean(reported_noise);
    const auto reported_looks_valid = std::any_of(reported_noise.begin(), reported_noise.end(), [](double value) {
        return std::abs(value) > 0.000001;
    });

    switch (config.estimator) {
    case NoiseEstimator::median:
        estimate.floor_dbm = percentile(values, 50.0);
        estimate.floor_by_bin = constant_floor(slice, estimate.floor_dbm);
        break;
    case NoiseEstimator::trimmed_mean:
        estimate.floor_dbm = trimmed_mean(values, config.trim_ratio);
        estimate.floor_by_bin = constant_floor(slice, estimate.floor_dbm);
        break;
    case NoiseEstimator::percentile:
        estimate.floor_dbm = percentile(values, config.percentile);
        estimate.floor_by_bin = constant_floor(slice, estimate.floor_dbm);
        break;
    case NoiseEstimator::rolling_percentile:
        estimate.floor_by_bin = rolling_percentile_floor(slice, config);
        estimate.floor_by_bin = smooth_values(std::move(estimate.floor_by_bin), config.smoothing_bins);
        estimate.floor_dbm = mean(estimate.floor_by_bin);
        break;
    }

    if (reported_looks_valid) {
        auto reported_floor = reported_mean;
        estimate.floor_dbm = std::min(estimate.floor_dbm, reported_floor + 3.0);
        if (estimate.floor_by_bin.empty()) {
            estimate.floor_by_bin = reported_noise;
        } else {
            for (std::size_t i = 0; i < estimate.floor_by_bin.size() && i < reported_noise.size(); ++i) {
                estimate.floor_by_bin[i] = std::min(estimate.floor_by_bin[i], reported_noise[i] + 3.0);
            }
        }
    }

    estimate.median_dbm = percentile(values, 50.0);
    estimate.deviation_db = standard_deviation(estimate.floor_by_bin.empty() ? values : estimate.floor_by_bin, estimate.floor_dbm);
    estimate.slope_db_per_mhz = slope_db_per_mhz(slice, estimate.floor_by_bin);
    return estimate;
}

std::vector<SpectralPeak> detect_spectral_peaks(const SpectrumSlice& slice, const PeakDetectorConfig& config) {
    std::vector<SpectralPeak> peaks;
    if (slice.points.empty()) {
        return peaks;
    }
    auto floor = estimate_noise_floor(slice, config.noise);
    if (floor.floor_by_bin.size() != slice.points.size()) {
        floor.floor_by_bin = constant_floor(slice, floor.floor_dbm);
    }

    for (std::size_t i = 0; i < slice.points.size(); ++i) {
        if (!local_maximum(slice, i)) {
            continue;
        }
        auto prominence = slice.points[i].power_dbm - floor.floor_by_bin[i];
        if (prominence < config.min_prominence_db || prominence < config.min_snr_db) {
            continue;
        }
        auto peak = make_peak(slice, floor.floor_by_bin, i, config);
        auto width_bins = peak.upper_bin >= peak.lower_bin ? peak.upper_bin - peak.lower_bin + 1 : 0;
        if (width_bins < config.min_width_bins) {
            continue;
        }
        peaks.push_back(peak);
    }

    peaks = merge_close_peaks(std::move(peaks), config.merge_gap_hz);
    std::sort(peaks.begin(), peaks.end(), [](const auto& left, const auto& right) {
        if (left.prominence_db != right.prominence_db) {
            return left.prominence_db > right.prominence_db;
        }
        return left.peak_power_dbm > right.peak_power_dbm;
    });
    if (peaks.size() > config.max_peaks) {
        peaks.resize(config.max_peaks);
    }
    std::sort(peaks.begin(), peaks.end(), [](const auto& left, const auto& right) {
        return left.center_frequency_hz < right.center_frequency_hz;
    });
    return peaks;
}

SpectrumOccupancy estimate_occupancy(const SpectrumSlice& slice,
                                      const NoiseFloorEstimate& floor,
                                      double occupied_margin_db) {
    SpectrumOccupancy occupancy;
    if (slice.points.empty()) {
        return occupancy;
    }
    occupancy.bins.reserve(slice.points.size());
    double occupied = 0.0;
    double margin_total = 0.0;
    for (std::size_t i = 0; i < slice.points.size(); ++i) {
        auto floor_value = floor.floor_by_bin.size() == slice.points.size() ? floor.floor_by_bin[i] : floor.floor_dbm;
        OccupancyBin bin;
        bin.frequency_hz = slice.points[i].frequency_hz;
        bin.power_dbm = slice.points[i].power_dbm;
        bin.floor_dbm = floor_value;
        bin.margin_db = bin.power_dbm - floor_value;
        bin.occupied = bin.margin_db >= occupied_margin_db;
        if (bin.occupied) {
            occupied += 1.0;
        }
        margin_total += bin.margin_db;
        occupancy.max_margin_db = std::max(occupancy.max_margin_db, bin.margin_db);
        occupancy.bins.push_back(bin);
    }
    occupancy.occupied_ratio = occupied / static_cast<double>(slice.points.size());
    occupancy.mean_margin_db = margin_total / static_cast<double>(slice.points.size());
    return occupancy;
}

SpectralAnalysis analyze_spectrum(const SpectrumSlice& slice, const PeakDetectorConfig& config) {
    SpectralAnalysis analysis;
    analysis.slice = slice;
    analysis.noise = estimate_noise_floor(slice, config.noise);
    analysis.peaks = detect_spectral_peaks(slice, config);
    analysis.occupancy = estimate_occupancy(slice, analysis.noise, config.min_snr_db);
    analysis.stats = telemetry::analyze_spectrum_frame(spectrum_frame_from_slice(slice));
    analysis.anomaly_report = telemetry::analyze_spectrum_anomalies(analysis.stats);
    return analysis;
}

SpectralAnalysis analyze_spectrum_frame(const telemetry::SpectrumFrame& frame, const PeakDetectorConfig& config) {
    return analyze_spectrum(spectrum_slice_from_frame(frame), config);
}

SpectrumSlice smooth_spectrum(const SpectrumSlice& slice, std::size_t window_bins) {
    auto smoothed = slice;
    auto values = powers(slice);
    values = smooth_values(std::move(values), window_bins);
    for (std::size_t i = 0; i < smoothed.points.size() && i < values.size(); ++i) {
        smoothed.points[i].power_dbm = values[i];
    }
    return smoothed;
}

SpectrumSlice subtract_noise_floor(const SpectrumSlice& slice, const NoiseFloorEstimate& floor) {
    auto result = slice;
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        auto floor_value = floor.floor_by_bin.size() == result.points.size() ? floor.floor_by_bin[i] : floor.floor_dbm;
        result.points[i].power_dbm -= floor_value;
        result.points[i].noise_dbm = 0.0;
    }
    return result;
}

double integrated_power_dbm(const std::vector<SpectrumPoint>& points, std::size_t first, std::size_t last) {
    if (points.empty() || first >= points.size()) {
        return -180.0;
    }
    last = std::min(last, points.size() - 1);
    if (first > last) {
        return -180.0;
    }
    double total_mw = 0.0;
    for (auto i = first; i <= last; ++i) {
        total_mw += dbm_to_mw(points[i].power_dbm);
    }
    return mw_to_dbm(total_mw);
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (values.size() == 1) {
        return values.front();
    }
    auto rank = clamp_percentile(p) / 100.0 * static_cast<double>(values.size() - 1);
    auto lower_index = static_cast<std::size_t>(std::floor(rank));
    auto upper_index = static_cast<std::size_t>(std::ceil(rank));
    auto fraction = rank - static_cast<double>(lower_index);
    return values[lower_index] + (values[upper_index] - values[lower_index]) * fraction;
}

std::string render_noise_floor(const NoiseFloorEstimate& estimate) {
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(2)
        << "noise_floor floor_dbm="
        << estimate.floor_dbm
        << " median_dbm="
        << estimate.median_dbm
        << " deviation_db="
        << estimate.deviation_db
        << " slope_db_per_mhz="
        << estimate.slope_db_per_mhz
        << " samples="
        << estimate.samples;
    return out.str();
}

std::string render_peak(const SpectralPeak& peak) {
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(2)
        << "peak center="
        << format_frequency(peak.center_frequency_hz)
        << " range="
        << format_frequency(peak.lower_frequency_hz)
        << "-"
        << format_frequency(peak.upper_frequency_hz)
        << " power_dbm="
        << peak.peak_power_dbm
        << " floor_dbm="
        << peak.noise_floor_dbm
        << " prominence_db="
        << peak.prominence_db
        << " integrated_dbm="
        << peak.integrated_power_dbm
        << " shape="
        << peak_shape_name(peak.shape);
    return out.str();
}

std::string render_spectral_analysis(const SpectralAnalysis& analysis) {
    std::ostringstream out;
    out << "spectral_analysis\n"
        << "  center: "
        << format_frequency(analysis.slice.center_frequency_hz)
        << "\n"
        << "  span: "
        << format_frequency(analysis.slice.span_hz)
        << "\n"
        << "  bins: "
        << analysis.slice.points.size()
        << "\n"
        << "  "
        << render_noise_floor(analysis.noise)
        << "\n"
        << "  occupancy_ratio: "
        << analysis.occupancy.occupied_ratio
        << "\n"
        << "  peaks: "
        << analysis.peaks.size()
        << "\n";
    for (const auto& peak : analysis.peaks) {
        out << "    " << render_peak(peak) << "\n";
    }
    if (!analysis.anomaly_report.ok()) {
        out << "  anomalies:\n";
        for (const auto& finding : analysis.anomaly_report.findings) {
            out << "    "
                << finding.code
                << " score="
                << finding.score
                << " "
                << finding.message
                << "\n";
        }
    }
    return out.str();
}

} // namespace aethon::rf
