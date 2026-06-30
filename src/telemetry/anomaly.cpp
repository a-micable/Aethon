#include "aethon/telemetry/anomaly.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::telemetry {
namespace {

void add_finding(AnomalyReport& report, std::string code, std::string message, double score) {
    report.findings.push_back(AnomalyFinding{
        std::move(code),
        std::move(message),
        score,
    });
    report.score = std::max(report.score, score);
}

double degraded_ratio(const ObservationStats& stats) {
    if (stats.total_readings == 0) {
        return 0.0;
    }
    return static_cast<double>(stats.degraded_readings)
        / static_cast<double>(stats.total_readings);
}

} // namespace

AnomalyReport analyze_observation_anomalies(const ObservationStats& stats,
                                            const AnomalyRuleSet& rules) {
    AnomalyReport report;
    auto ratio = degraded_ratio(stats);
    if (ratio > rules.max_degraded_ratio) {
        add_finding(
            report,
            "observation.degraded_ratio",
            "too many scalar readings have degraded quality",
            ratio);
    }
    for (const auto& channel : stats.channels) {
        auto stddev = standard_deviation(channel.values);
        if (stddev == 0.0 || channel.values.count == 0) {
            continue;
        }
        auto z_min = std::abs((channel.values.min - channel.values.mean) / stddev);
        auto z_max = std::abs((channel.values.max - channel.values.mean) / stddev);
        auto z = std::max(z_min, z_max);
        if (z > rules.max_abs_zscore) {
            std::ostringstream message;
            message << "channel "
                    << channel.channel
                    << " has extreme z-score "
                    << z;
            add_finding(report, "observation.channel_outlier", message.str(), z);
        }
    }
    return report;
}

AnomalyReport analyze_spectrum_anomalies(const SpectrumStats& stats,
                                         const AnomalyRuleSet& rules) {
    AnomalyReport report;
    if (stats.power_dbm.count == 0 || stats.noise_dbm.count == 0) {
        add_finding(report, "spectrum.empty", "spectrum statistics are empty", 1.0);
        return report;
    }
    auto snr = stats.power_dbm.mean - stats.noise_dbm.mean;
    if (snr < rules.min_spectrum_snr_db) {
        add_finding(report, "spectrum.low_snr", "mean signal-to-noise ratio is low", rules.min_spectrum_snr_db - snr);
    }
    if (stats.power_dbm.max >= 0.0) {
        add_finding(report, "spectrum.saturated", "spectrum reports non-negative RF power", stats.power_dbm.max);
    }
    return report;
}

std::string render_anomaly_report(const AnomalyReport& report) {
    std::ostringstream out;
    out << "anomaly_report\n"
        << "  ok: "
        << (report.ok() ? "yes" : "no")
        << "\n"
        << "  score: "
        << report.score
        << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: "
            << finding.code
            << " score="
            << finding.score
            << " "
            << finding.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::telemetry
