#include "aethon/rf/iq_summary.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace aethon::rf {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

double db(double ratio) {
    if (ratio <= 0.0) {
        return -120.0;
    }
    return 20.0 * std::log10(ratio);
}

double wrap_phase(double phase) {
    while (phase > kPi) {
        phase -= 2.0 * kPi;
    }
    while (phase < -kPi) {
        phase += 2.0 * kPi;
    }
    return phase;
}

double mean_i(const IqWindow& window) {
    if (window.samples.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& sample : window.samples) {
        total += sample.i;
    }
    return total / static_cast<double>(window.samples.size());
}

double mean_q(const IqWindow& window) {
    if (window.samples.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& sample : window.samples) {
        total += sample.q;
    }
    return total / static_cast<double>(window.samples.size());
}

double rms_component(const IqWindow& window, bool use_i) {
    if (window.samples.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (const auto& sample : window.samples) {
        auto value = use_i ? sample.i : sample.q;
        total += value * value;
    }
    return std::sqrt(total / static_cast<double>(window.samples.size()));
}

IqWindow slice_window(const IqWindow& window, std::size_t first, std::size_t count) {
    IqWindow result;
    result.center_frequency_hz = window.center_frequency_hz;
    result.sample_rate_hz = window.sample_rate_hz;
    auto end = std::min(window.samples.size(), first + count);
    if (first >= end) {
        return result;
    }
    result.samples.insert(result.samples.end(), window.samples.begin() + static_cast<std::ptrdiff_t>(first), window.samples.begin() + static_cast<std::ptrdiff_t>(end));
    return result;
}

} // namespace

IqSummary summarize_iq(const IqWindow& window, const IqSummaryConfig& config) {
    IqSummary summary;
    summary.sample_count = window.samples.size();
    if (window.samples.empty()) {
        return summary;
    }

    auto dc_window = config.remove_dc_for_offset_estimate ? remove_dc(window) : window;
    double magnitude_square_total = 0.0;
    double clipped = 0.0;
    double zero_crossings = 0.0;
    double previous_i = window.samples.front().i;

    for (const auto& sample : dc_window.samples) {
        telemetry::add_sample(summary.i, sample.i);
        telemetry::add_sample(summary.q, sample.q);
        auto mag = magnitude(sample);
        auto phase = phase_radians(sample);
        telemetry::add_sample(summary.magnitude, mag);
        telemetry::add_sample(summary.phase_radians, phase);
        magnitude_square_total += mag * mag;
        summary.peak_magnitude = std::max(summary.peak_magnitude, mag);
        if (std::abs(sample.i) >= config.clipping_threshold || std::abs(sample.q) >= config.clipping_threshold || mag >= config.clipping_threshold) {
            clipped += 1.0;
        }
        if ((previous_i < -config.zero_crossing_deadband && sample.i > config.zero_crossing_deadband)
            || (previous_i > config.zero_crossing_deadband && sample.i < -config.zero_crossing_deadband)) {
            zero_crossings += 1.0;
        }
        previous_i = sample.i;
    }

    summary.mean_i = mean_i(window);
    summary.mean_q = mean_q(window);
    summary.rms = std::sqrt(magnitude_square_total / static_cast<double>(window.samples.size()));
    summary.dc_offset_magnitude = std::hypot(summary.mean_i, summary.mean_q);
    summary.crest_factor_db = summary.rms > 0.0 ? db(summary.peak_magnitude / summary.rms) : 0.0;
    summary.iq_gain_imbalance_db = estimate_iq_gain_imbalance_db(window);
    summary.iq_phase_error_degrees = estimate_iq_phase_error_degrees(window);
    summary.clipping_ratio = clipped / static_cast<double>(window.samples.size());
    summary.zero_crossing_rate = zero_crossings / static_cast<double>(window.samples.size());
    summary.estimated_frequency_offset_hz = estimate_frequency_offset_hz(window);
    return summary;
}

std::vector<IqSegmentSummary> summarize_iq_segments(const IqWindow& window,
                                                    std::size_t segment_size,
                                                    const IqSummaryConfig& config) {
    std::vector<IqSegmentSummary> summaries;
    if (segment_size == 0) {
        return summaries;
    }
    for (std::size_t first = 0; first < window.samples.size(); first += segment_size) {
        auto segment = slice_window(window, first, segment_size);
        IqSegmentSummary summary;
        summary.first_sample = first;
        summary.sample_count = segment.samples.size();
        summary.summary = summarize_iq(segment, config);
        summaries.push_back(summary);
    }
    return summaries;
}

IqWindow remove_dc(const IqWindow& window) {
    IqWindow result = window;
    auto mi = mean_i(window);
    auto mq = mean_q(window);
    for (auto& sample : result.samples) {
        sample.i -= mi;
        sample.q -= mq;
    }
    return result;
}

IqWindow normalize_iq(const IqWindow& window, double target_rms) {
    auto result = window;
    auto summary = summarize_iq(remove_dc(window));
    if (summary.rms <= 0.0) {
        return result;
    }
    auto scale = target_rms / summary.rms;
    for (auto& sample : result.samples) {
        sample.i *= scale;
        sample.q *= scale;
    }
    return result;
}

double estimate_frequency_offset_hz(const IqWindow& window) {
    if (window.samples.size() < 2 || window.sample_rate_hz == 0) {
        return 0.0;
    }
    auto corrected = remove_dc(window);
    double total_delta = 0.0;
    std::size_t count = 0;
    auto previous = phase_radians(corrected.samples.front());
    for (std::size_t i = 1; i < corrected.samples.size(); ++i) {
        auto current = phase_radians(corrected.samples[i]);
        total_delta += wrap_phase(current - previous);
        previous = current;
        ++count;
    }
    if (count == 0) {
        return 0.0;
    }
    auto mean_delta = total_delta / static_cast<double>(count);
    return mean_delta * static_cast<double>(window.sample_rate_hz) / (2.0 * kPi);
}

double estimate_iq_phase_error_degrees(const IqWindow& window) {
    if (window.samples.empty()) {
        return 0.0;
    }
    auto corrected = remove_dc(window);
    double ii = 0.0;
    double qq = 0.0;
    double iq = 0.0;
    for (const auto& sample : corrected.samples) {
        ii += sample.i * sample.i;
        qq += sample.q * sample.q;
        iq += sample.i * sample.q;
    }
    if (ii <= 0.0 || qq <= 0.0) {
        return 0.0;
    }
    auto correlation = std::clamp(iq / std::sqrt(ii * qq), -1.0, 1.0);
    return std::asin(correlation) * 180.0 / kPi;
}

double estimate_iq_gain_imbalance_db(const IqWindow& window) {
    auto i_rms = rms_component(remove_dc(window), true);
    auto q_rms = rms_component(remove_dc(window), false);
    if (i_rms <= 0.0 || q_rms <= 0.0) {
        return 0.0;
    }
    return db(i_rms / q_rms);
}

double magnitude(const IqSample& sample) {
    return std::hypot(sample.i, sample.q);
}

double phase_radians(const IqSample& sample) {
    return std::atan2(sample.q, sample.i);
}

std::string render_iq_summary(const IqSummary& summary) {
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(4)
        << "iq_summary samples="
        << summary.sample_count
        << " mean_i="
        << summary.mean_i
        << " mean_q="
        << summary.mean_q
        << " rms="
        << summary.rms
        << " peak="
        << summary.peak_magnitude
        << " crest_db="
        << summary.crest_factor_db
        << " dc="
        << summary.dc_offset_magnitude
        << " gain_imbalance_db="
        << summary.iq_gain_imbalance_db
        << " phase_error_deg="
        << summary.iq_phase_error_degrees
        << " clipping="
        << summary.clipping_ratio
        << " freq_offset_hz="
        << summary.estimated_frequency_offset_hz;
    return out.str();
}

std::string render_iq_segment_summaries(const std::vector<IqSegmentSummary>& summaries) {
    std::ostringstream out;
    out << "iq_segments\n"
        << "  count: "
        << summaries.size()
        << "\n";
    for (const auto& summary : summaries) {
        out << "  segment first="
            << summary.first_sample
            << " samples="
            << summary.sample_count
            << " "
            << render_iq_summary(summary.summary)
            << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
