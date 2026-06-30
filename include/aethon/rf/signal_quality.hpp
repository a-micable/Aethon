#pragma once

#include "aethon/rf/interference.hpp"
#include "aethon/rf/iq_summary.hpp"
#include "aethon/rf/sweep_planner.hpp"

#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

struct SignalQualityConfig {
    double excellent_snr_db = 30.0;
    double usable_snr_db = 10.0;
    double max_good_occupancy = 0.35;
    double max_clipping_ratio = 0.01;
    double max_dc_offset = 0.08;
    double max_iq_gain_imbalance_db = 1.5;
    double max_iq_phase_error_degrees = 3.0;
    double overload_power_dbm = -5.0;
};

struct SignalQualityInput {
    SpectralAnalysis spectral;
    InterferenceReport interference;
    std::optional<IqSummary> iq;
    std::optional<BandAllocation> allocation;
    std::optional<SweepSegment> sweep_segment;
};

[[nodiscard]] SignalQualityScore score_signal_quality(const SignalQualityInput& input,
                                                      const SignalQualityConfig& config = {});
[[nodiscard]] SignalQualityScore score_spectrum_quality(const SpectralAnalysis& analysis,
                                                       const BandPlan& band_plan,
                                                       const SignalQualityConfig& config = {});
[[nodiscard]] SignalQualityScore score_spectrum_frame_quality(const telemetry::SpectrumFrame& frame,
                                                             const BandPlan& band_plan,
                                                             const SignalQualityConfig& config = {});
[[nodiscard]] double score_snr(double snr_db, const SignalQualityConfig& config);
[[nodiscard]] double score_occupancy(double occupancy_ratio, const SignalQualityConfig& config);
[[nodiscard]] double score_iq_health(const IqSummary& summary, const SignalQualityConfig& config);
[[nodiscard]] std::string quality_grade(double score);
[[nodiscard]] std::string render_signal_quality(const SignalQualityScore& score);

} // namespace aethon::rf
