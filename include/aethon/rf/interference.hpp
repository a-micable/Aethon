#pragma once

#include "aethon/rf/band_plan.hpp"
#include "aethon/rf/spectral_analysis.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class InterferenceSeverity : std::uint8_t {
    info = 0,
    low = 1,
    medium = 2,
    high = 3,
    critical = 4,
};

struct ExpectedSignal {
    FrequencyRange occupied_range;
    RfService service = RfService::unknown;
    std::uint64_t center_frequency_hz = 0;
    std::uint64_t bandwidth_hz = 0;
    double nominal_power_dbm = 0.0;
    double tolerance_db = 6.0;
    std::string label;
};

struct InterferenceFinding {
    InterferenceKind kind = InterferenceKind::none;
    InterferenceSeverity severity = InterferenceSeverity::info;
    FrequencyRange range;
    double confidence = 0.0;
    double excess_db = 0.0;
    std::optional<SpectralPeak> peak;
    std::optional<BandAllocation> allocation;
    std::string summary;
    std::vector<std::string> evidence;
};

struct InterferenceRules {
    double narrowband_prominence_db = 12.0;
    double broadband_occupancy_ratio = 0.55;
    double broadband_floor_rise_db = 8.0;
    double adjacent_guard_fraction = 0.35;
    double overload_power_dbm = -5.0;
    double intermod_spacing_tolerance_hz = 2'000.0;
    bool flag_passive_band_emissions = true;
};

struct InterferenceReport {
    std::vector<InterferenceFinding> findings;
    double worst_confidence = 0.0;
    InterferenceSeverity worst_severity = InterferenceSeverity::info;
    [[nodiscard]] bool ok() const noexcept {
        return findings.empty();
    }
};

[[nodiscard]] InterferenceReport classify_interference(const SpectralAnalysis& analysis,
                                                       const BandPlan& band_plan,
                                                       const std::vector<ExpectedSignal>& expected = {},
                                                       const InterferenceRules& rules = {});
[[nodiscard]] InterferenceReport classify_interference(const telemetry::SpectrumFrame& frame,
                                                       const BandPlan& band_plan,
                                                       const std::vector<ExpectedSignal>& expected = {},
                                                       const InterferenceRules& rules = {});
[[nodiscard]] std::vector<InterferenceFinding> classify_peak_interference(const SpectralAnalysis& analysis,
                                                                          const BandPlan& band_plan,
                                                                          const InterferenceRules& rules);
[[nodiscard]] std::vector<InterferenceFinding> detect_intermodulation_products(const std::vector<SpectralPeak>& peaks,
                                                                               const InterferenceRules& rules);
[[nodiscard]] bool overlaps_expected_signal(const SpectralPeak& peak, const std::vector<ExpectedSignal>& expected);
[[nodiscard]] double severity_score(InterferenceSeverity severity);
[[nodiscard]] std::string interference_severity_name(InterferenceSeverity severity);
[[nodiscard]] std::string render_interference_finding(const InterferenceFinding& finding);
[[nodiscard]] std::string render_interference_report(const InterferenceReport& report);

} // namespace aethon::rf
