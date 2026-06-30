#include "aethon/rf/interference.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

FrequencyRange peak_range(const SpectralPeak& peak) {
    return {peak.lower_frequency_hz, peak.upper_frequency_hz + 1};
}

FrequencyRange expected_range(const ExpectedSignal& signal) {
    if (!signal.occupied_range.empty()) {
        return signal.occupied_range;
    }
    auto half = signal.bandwidth_hz / 2;
    return {
        signal.center_frequency_hz > half ? signal.center_frequency_hz - half : 0,
        signal.center_frequency_hz + half + (signal.bandwidth_hz % 2),
    };
}

InterferenceSeverity severity_from_excess(double excess_db) {
    if (excess_db >= 30.0) {
        return InterferenceSeverity::critical;
    }
    if (excess_db >= 20.0) {
        return InterferenceSeverity::high;
    }
    if (excess_db >= 10.0) {
        return InterferenceSeverity::medium;
    }
    if (excess_db >= 3.0) {
        return InterferenceSeverity::low;
    }
    return InterferenceSeverity::info;
}

InterferenceSeverity max_severity(InterferenceSeverity left, InterferenceSeverity right) {
    return severity_score(left) >= severity_score(right) ? left : right;
}

double clamp_confidence(double value) {
    return std::clamp(value, 0.0, 1.0);
}

void add_evidence(InterferenceFinding& finding, std::string text) {
    finding.evidence.push_back(std::move(text));
}

InterferenceFinding make_peak_finding(InterferenceKind kind,
                                      InterferenceSeverity severity,
                                      const SpectralPeak& peak,
                                      std::string summary) {
    InterferenceFinding finding;
    finding.kind = kind;
    finding.severity = severity;
    finding.range = peak_range(peak);
    finding.excess_db = peak.prominence_db;
    finding.confidence = clamp_confidence(0.35 + peak.prominence_db / 40.0);
    finding.peak = peak;
    finding.summary = std::move(summary);
    add_evidence(finding, "peak " + format_frequency(peak.center_frequency_hz) + " prominence " + std::to_string(peak.prominence_db) + " dB");
    add_evidence(finding, "shape " + peak_shape_name(peak.shape));
    return finding;
}

bool in_guard(const BandPlan& plan, const BandAllocation& allocation, const SpectralPeak& peak) {
    auto range = peak_range(peak);
    for (const auto& guard : plan.guard_ranges(allocation)) {
        if (range.overlaps(guard)) {
            return true;
        }
    }
    return false;
}

std::optional<BandAllocation> allocation_for_peak(const BandPlan& plan, const SpectralPeak& peak) {
    auto center = plan.best_allocation(peak.center_frequency_hz);
    if (center) {
        return center;
    }
    auto lower = plan.best_allocation(peak.lower_frequency_hz);
    if (lower) {
        return lower;
    }
    return plan.best_allocation(peak.upper_frequency_hz);
}

bool passive_allocation(const BandAllocation& allocation) {
    return allocation.status == AllocationStatus::protected_passive
        || allocation.duplex == DuplexMode::receive_only
        || allocation.service == RfService::navigation
        || allocation.service == RfService::weather;
}

std::string allocation_text(const BandAllocation& allocation) {
    return allocation.name + " (" + rf_service_name(allocation.service) + ")";
}

void append_findings(InterferenceReport& report, std::vector<InterferenceFinding> findings) {
    for (auto& finding : findings) {
        report.worst_confidence = std::max(report.worst_confidence, finding.confidence);
        report.worst_severity = max_severity(report.worst_severity, finding.severity);
        report.findings.push_back(std::move(finding));
    }
}

std::vector<InterferenceFinding> classify_expected_mismatches(const SpectralAnalysis& analysis,
                                                              const std::vector<ExpectedSignal>& expected) {
    std::vector<InterferenceFinding> findings;
    for (const auto& peak : analysis.peaks) {
        for (const auto& signal : expected) {
            auto expected_occupied = expected_range(signal);
            if (!peak_range(peak).overlaps(expected_occupied)) {
                continue;
            }
            auto excess = std::abs(peak.peak_power_dbm - signal.nominal_power_dbm) - signal.tolerance_db;
            if (excess <= 0.0) {
                continue;
            }
            auto finding = make_peak_finding(
                InterferenceKind::co_channel,
                severity_from_excess(excess),
                peak,
                "expected signal deviates from nominal power");
            finding.excess_db = excess;
            finding.confidence = clamp_confidence(0.5 + excess / 30.0);
            add_evidence(finding, "expected " + signal.label + " nominal " + std::to_string(signal.nominal_power_dbm) + " dBm");
            findings.push_back(std::move(finding));
        }
    }
    return findings;
}

std::vector<InterferenceFinding> classify_broadband(const SpectralAnalysis& analysis,
                                                    const InterferenceRules& rules) {
    std::vector<InterferenceFinding> findings;
    if (analysis.slice.points.empty()) {
        return findings;
    }
    auto floor_rise = analysis.noise.floor_dbm - analysis.noise.median_dbm;
    if (analysis.occupancy.occupied_ratio < rules.broadband_occupancy_ratio && floor_rise < rules.broadband_floor_rise_db) {
        return findings;
    }
    InterferenceFinding finding;
    finding.kind = InterferenceKind::broadband_noise;
    finding.severity = severity_from_excess(std::max(floor_rise, analysis.occupancy.mean_margin_db));
    finding.range = {
        analysis.slice.points.front().frequency_hz,
        analysis.slice.points.back().frequency_hz + analysis.slice.bin_width_hz,
    };
    finding.excess_db = std::max(floor_rise, analysis.occupancy.mean_margin_db);
    finding.confidence = clamp_confidence(analysis.occupancy.occupied_ratio + std::max(0.0, floor_rise) / 25.0);
    finding.summary = "broadband energy raises occupancy or apparent noise floor";
    add_evidence(finding, "occupied ratio " + std::to_string(analysis.occupancy.occupied_ratio));
    add_evidence(finding, "floor rise " + std::to_string(floor_rise) + " dB");
    findings.push_back(std::move(finding));
    return findings;
}

} // namespace

InterferenceReport classify_interference(const SpectralAnalysis& analysis,
                                         const BandPlan& band_plan,
                                         const std::vector<ExpectedSignal>& expected,
                                         const InterferenceRules& rules) {
    InterferenceReport report;
    append_findings(report, classify_broadband(analysis, rules));
    append_findings(report, classify_peak_interference(analysis, band_plan, rules));
    append_findings(report, classify_expected_mismatches(analysis, expected));
    append_findings(report, detect_intermodulation_products(analysis.peaks, rules));

    std::sort(report.findings.begin(), report.findings.end(), [](const auto& left, const auto& right) {
        if (severity_score(left.severity) != severity_score(right.severity)) {
            return severity_score(left.severity) > severity_score(right.severity);
        }
        return left.confidence > right.confidence;
    });
    return report;
}

InterferenceReport classify_interference(const telemetry::SpectrumFrame& frame,
                                         const BandPlan& band_plan,
                                         const std::vector<ExpectedSignal>& expected,
                                         const InterferenceRules& rules) {
    return classify_interference(rf::analyze_spectrum_frame(frame), band_plan, expected, rules);
}

std::vector<InterferenceFinding> classify_peak_interference(const SpectralAnalysis& analysis,
                                                            const BandPlan& band_plan,
                                                            const InterferenceRules& rules) {
    std::vector<InterferenceFinding> findings;
    for (const auto& peak : analysis.peaks) {
        auto allocation = allocation_for_peak(band_plan, peak);
        if (peak.peak_power_dbm >= rules.overload_power_dbm) {
            auto finding = make_peak_finding(InterferenceKind::overload, InterferenceSeverity::critical, peak, "very high RF power suggests receiver overload risk");
            if (allocation) {
                finding.allocation = allocation;
                add_evidence(finding, "inside " + allocation_text(*allocation));
            }
            findings.push_back(std::move(finding));
            continue;
        }

        if (peak.shape == PeakShape::narrowband && peak.prominence_db >= rules.narrowband_prominence_db) {
            auto finding = make_peak_finding(InterferenceKind::narrowband_carrier, severity_from_excess(peak.prominence_db), peak, "unexpected narrowband carrier");
            if (allocation) {
                finding.allocation = allocation;
                add_evidence(finding, "inside " + allocation_text(*allocation));
            }
            findings.push_back(std::move(finding));
        }

        if (allocation && in_guard(band_plan, *allocation, peak)) {
            auto finding = make_peak_finding(InterferenceKind::adjacent_channel, InterferenceSeverity::medium, peak, "energy falls in allocation guard range");
            finding.allocation = allocation;
            add_evidence(finding, "guard range of " + allocation_text(*allocation));
            findings.push_back(std::move(finding));
        }

        if (allocation && rules.flag_passive_band_emissions && passive_allocation(*allocation) && peak.prominence_db >= 6.0) {
            auto finding = make_peak_finding(InterferenceKind::co_channel, InterferenceSeverity::high, peak, "emission observed in protected or receive-only allocation");
            finding.allocation = allocation;
            add_evidence(finding, "passive allocation " + allocation_text(*allocation));
            findings.push_back(std::move(finding));
        }
    }
    return findings;
}

std::vector<InterferenceFinding> detect_intermodulation_products(const std::vector<SpectralPeak>& peaks,
                                                                 const InterferenceRules& rules) {
    std::vector<InterferenceFinding> findings;
    if (peaks.size() < 3) {
        return findings;
    }
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        for (std::size_t j = i + 1; j < peaks.size(); ++j) {
            auto f1 = static_cast<double>(peaks[i].center_frequency_hz);
            auto f2 = static_cast<double>(peaks[j].center_frequency_hz);
            std::vector<double> products = {
                2.0 * f1 - f2,
                2.0 * f2 - f1,
                f1 + f2,
                std::abs(f2 - f1),
            };
            for (auto product : products) {
                if (product <= 0.0) {
                    continue;
                }
                for (const auto& peak : peaks) {
                    auto delta = std::abs(static_cast<double>(peak.center_frequency_hz) - product);
                    if (delta > rules.intermod_spacing_tolerance_hz) {
                        continue;
                    }
                    if (peak.center_frequency_hz == peaks[i].center_frequency_hz || peak.center_frequency_hz == peaks[j].center_frequency_hz) {
                        continue;
                    }
                    auto finding = make_peak_finding(InterferenceKind::intermodulation, InterferenceSeverity::medium, peak, "peak aligns with two-tone intermodulation product");
                    finding.confidence = clamp_confidence(0.55 + peak.prominence_db / 50.0);
                    add_evidence(finding, "source tones " + format_frequency(peaks[i].center_frequency_hz) + " and " + format_frequency(peaks[j].center_frequency_hz));
                    findings.push_back(std::move(finding));
                }
            }
        }
    }
    return findings;
}

bool overlaps_expected_signal(const SpectralPeak& peak, const std::vector<ExpectedSignal>& expected) {
    auto range = peak_range(peak);
    return std::any_of(expected.begin(), expected.end(), [&](const auto& signal) {
        return range.overlaps(expected_range(signal));
    });
}

double severity_score(InterferenceSeverity severity) {
    switch (severity) {
    case InterferenceSeverity::info:
        return 0.0;
    case InterferenceSeverity::low:
        return 1.0;
    case InterferenceSeverity::medium:
        return 2.0;
    case InterferenceSeverity::high:
        return 3.0;
    case InterferenceSeverity::critical:
        return 4.0;
    }
    return 0.0;
}

std::string interference_severity_name(InterferenceSeverity severity) {
    switch (severity) {
    case InterferenceSeverity::info:
        return "info";
    case InterferenceSeverity::low:
        return "low";
    case InterferenceSeverity::medium:
        return "medium";
    case InterferenceSeverity::high:
        return "high";
    case InterferenceSeverity::critical:
        return "critical";
    }
    return "info";
}

std::string render_interference_finding(const InterferenceFinding& finding) {
    std::ostringstream out;
    out << interference_kind_name(finding.kind)
        << " severity="
        << interference_severity_name(finding.severity)
        << " confidence="
        << finding.confidence
        << " range="
        << format_range(finding.range)
        << " excess_db="
        << finding.excess_db
        << " summary=\""
        << finding.summary
        << "\"";
    if (finding.allocation) {
        out << " allocation=\"" << finding.allocation->name << "\"";
    }
    return out.str();
}

std::string render_interference_report(const InterferenceReport& report) {
    std::ostringstream out;
    out << "interference_report\n"
        << "  ok: "
        << (report.ok() ? "yes" : "no")
        << "\n"
        << "  worst_severity: "
        << interference_severity_name(report.worst_severity)
        << "\n"
        << "  worst_confidence: "
        << report.worst_confidence
        << "\n"
        << "  findings: "
        << report.findings.size()
        << "\n";
    for (const auto& finding : report.findings) {
        out << "  finding: " << render_interference_finding(finding) << "\n";
        for (const auto& evidence : finding.evidence) {
            out << "    evidence: " << evidence << "\n";
        }
    }
    return out.str();
}

} // namespace aethon::rf
