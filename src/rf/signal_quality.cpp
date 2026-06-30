#include "aethon/rf/signal_quality.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

void add_finding(SignalQualityScore& score, std::string finding) {
    score.findings.push_back(std::move(finding));
}

void add_recommendation(SignalQualityScore& score, std::string recommendation) {
    score.recommendations.push_back(std::move(recommendation));
}

double clamp_score(double value) {
    return std::clamp(value, 0.0, 100.0);
}

double max_peak_power(const SpectralAnalysis& analysis) {
    double max_power = analysis.stats.power_dbm.max;
    for (const auto& peak : analysis.peaks) {
        max_power = std::max(max_power, peak.peak_power_dbm);
    }
    return max_power;
}

double mean_snr(const SpectralAnalysis& analysis) {
    if (analysis.slice.points.empty()) {
        return 0.0;
    }
    return analysis.stats.power_dbm.mean - analysis.noise.floor_dbm;
}

double dynamic_range(const SpectralAnalysis& analysis) {
    if (analysis.stats.power_dbm.count == 0) {
        return 0.0;
    }
    return analysis.stats.power_dbm.max - analysis.noise.floor_dbm;
}

double interference_penalty(const InterferenceReport& report) {
    double penalty = 0.0;
    for (const auto& finding : report.findings) {
        penalty += severity_score(finding.severity) * 6.0 * std::max(0.25, finding.confidence);
    }
    return std::min(45.0, penalty);
}

double iq_penalty(const IqSummary& summary, const SignalQualityConfig& config, SignalQualityScore& score) {
    double penalty = 0.0;
    if (summary.clipping_ratio > config.max_clipping_ratio) {
        auto excess = summary.clipping_ratio - config.max_clipping_ratio;
        penalty += std::min(20.0, excess * 200.0);
        add_finding(score, "I/Q clipping ratio is elevated");
        add_recommendation(score, "reduce front-end gain or add attenuation");
    }
    if (summary.dc_offset_magnitude > config.max_dc_offset) {
        penalty += std::min(12.0, (summary.dc_offset_magnitude - config.max_dc_offset) * 80.0);
        add_finding(score, "I/Q DC offset is elevated");
        add_recommendation(score, "run DC calibration or enable DC blocking");
    }
    if (std::abs(summary.iq_gain_imbalance_db) > config.max_iq_gain_imbalance_db) {
        penalty += std::min(10.0, (std::abs(summary.iq_gain_imbalance_db) - config.max_iq_gain_imbalance_db) * 3.0);
        add_finding(score, "I/Q gain imbalance is above tolerance");
        add_recommendation(score, "calibrate I/Q gain balance");
    }
    if (std::abs(summary.iq_phase_error_degrees) > config.max_iq_phase_error_degrees) {
        penalty += std::min(10.0, (std::abs(summary.iq_phase_error_degrees) - config.max_iq_phase_error_degrees) * 2.0);
        add_finding(score, "I/Q phase error is above tolerance");
        add_recommendation(score, "calibrate quadrature phase");
    }
    return penalty;
}

void annotate_allocation(SignalQualityScore& score, const SignalQualityInput& input) {
    if (!input.allocation) {
        add_finding(score, "spectrum is outside known monitored allocation");
        return;
    }
    if (input.allocation->status == AllocationStatus::protected_passive) {
        add_finding(score, "allocation is protected passive");
        add_recommendation(score, "investigate any persistent emissions in this band");
    }
    if (input.allocation->service == RfService::navigation && !input.spectral.peaks.empty()) {
        add_recommendation(score, "verify GNSS/navigation front-end filtering and antenna placement");
    }
}

} // namespace

SignalQualityScore score_signal_quality(const SignalQualityInput& input, const SignalQualityConfig& config) {
    SignalQualityScore score;
    score.snr_db = mean_snr(input.spectral);
    score.dynamic_range_db = dynamic_range(input.spectral);
    score.occupancy_ratio = input.spectral.occupancy.occupied_ratio;
    score.interference_penalty = interference_penalty(input.interference);

    auto snr_points = score_snr(score.snr_db, config);
    auto occupancy_points = score_occupancy(score.occupancy_ratio, config);
    auto dynamic_points = clamp_score(score.dynamic_range_db / std::max(config.excellent_snr_db, 1.0) * 100.0);
    auto base = 0.45 * snr_points + 0.25 * occupancy_points + 0.20 * dynamic_points + 10.0;

    if (max_peak_power(input.spectral) >= config.overload_power_dbm) {
        score.stability_penalty += 25.0;
        add_finding(score, "peak power approaches receiver overload");
        add_recommendation(score, "insert attenuation or narrow preselector bandwidth");
    }
    if (!input.spectral.anomaly_report.ok()) {
        score.stability_penalty += std::min(15.0, input.spectral.anomaly_report.score * 3.0);
        add_finding(score, "telemetry anomaly rules flagged the spectrum");
    }
    if (input.iq) {
        score.stability_penalty += iq_penalty(*input.iq, config, score);
    }

    for (const auto& finding : input.interference.findings) {
        add_finding(score, interference_kind_name(finding.kind) + ": " + finding.summary);
        if (finding.kind == InterferenceKind::broadband_noise) {
            add_recommendation(score, "repeat sweep with antenna disconnected to separate local receiver noise from external noise");
        }
        if (finding.kind == InterferenceKind::adjacent_channel) {
            add_recommendation(score, "tighten channel filter or inspect adjacent transmitter mask");
        }
        if (finding.kind == InterferenceKind::intermodulation) {
            add_recommendation(score, "check front-end linearity and nearby high-power transmitters");
        }
    }

    annotate_allocation(score, input);
    score.score = clamp_score(base - score.interference_penalty - score.stability_penalty);
    if (score.score >= 90.0) {
        add_finding(score, "signal quality is excellent");
    } else if (score.score < 50.0) {
        add_recommendation(score, "collect a narrower follow-up sweep around strongest peaks");
    }
    return score;
}

SignalQualityScore score_spectrum_quality(const SpectralAnalysis& analysis,
                                          const BandPlan& band_plan,
                                          const SignalQualityConfig& config) {
    SignalQualityInput input;
    input.spectral = analysis;
    input.interference = classify_interference(analysis, band_plan);
    input.allocation = band_plan.best_allocation(analysis.slice.center_frequency_hz);
    return score_signal_quality(input, config);
}

SignalQualityScore score_spectrum_frame_quality(const telemetry::SpectrumFrame& frame,
                                                const BandPlan& band_plan,
                                                const SignalQualityConfig& config) {
    return score_spectrum_quality(rf::analyze_spectrum_frame(frame), band_plan, config);
}

double score_snr(double snr_db, const SignalQualityConfig& config) {
    if (snr_db <= config.usable_snr_db) {
        return clamp_score((snr_db / std::max(config.usable_snr_db, 1.0)) * 45.0);
    }
    auto span = std::max(config.excellent_snr_db - config.usable_snr_db, 1.0);
    return clamp_score(45.0 + (snr_db - config.usable_snr_db) / span * 55.0);
}

double score_occupancy(double occupancy_ratio, const SignalQualityConfig& config) {
    if (occupancy_ratio <= config.max_good_occupancy) {
        return 100.0;
    }
    auto excess = occupancy_ratio - config.max_good_occupancy;
    auto span = std::max(1.0 - config.max_good_occupancy, 0.01);
    return clamp_score(100.0 - excess / span * 80.0);
}

double score_iq_health(const IqSummary& summary, const SignalQualityConfig& config) {
    SignalQualityScore scratch;
    return clamp_score(100.0 - iq_penalty(summary, config, scratch));
}

std::string quality_grade(double score) {
    if (score >= 90.0) {
        return "excellent";
    }
    if (score >= 75.0) {
        return "good";
    }
    if (score >= 60.0) {
        return "fair";
    }
    if (score >= 40.0) {
        return "poor";
    }
    return "unusable";
}

std::string render_signal_quality(const SignalQualityScore& score) {
    std::ostringstream out;
    out << "signal_quality grade="
        << quality_grade(score.score)
        << " score="
        << score.score
        << " snr_db="
        << score.snr_db
        << " dynamic_range_db="
        << score.dynamic_range_db
        << " occupancy="
        << score.occupancy_ratio
        << " interference_penalty="
        << score.interference_penalty
        << " stability_penalty="
        << score.stability_penalty
        << "\n";
    for (const auto& finding : score.findings) {
        out << "  finding: " << finding << "\n";
    }
    for (const auto& recommendation : score.recommendations) {
        out << "  recommendation: " << recommendation << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
