#pragma once

#include "aethon/telemetry/statistics.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::telemetry {

struct AnomalyRuleSet {
    double max_abs_zscore = 6.0;
    double max_degraded_ratio = 0.25;
    double min_spectrum_snr_db = 3.0;
};

struct AnomalyFinding {
    std::string code;
    std::string message;
    double score = 0.0;
};

struct AnomalyReport {
    std::vector<AnomalyFinding> findings;
    double score = 0.0;

    [[nodiscard]] bool ok() const noexcept {
        return findings.empty();
    }
};

[[nodiscard]] AnomalyReport analyze_observation_anomalies(const ObservationStats& stats,
                                                          const AnomalyRuleSet& rules = {});
[[nodiscard]] AnomalyReport analyze_spectrum_anomalies(const SpectrumStats& stats,
                                                       const AnomalyRuleSet& rules = {});
[[nodiscard]] std::string render_anomaly_report(const AnomalyReport& report);

} // namespace aethon::telemetry
