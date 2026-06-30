#pragma once

#include "aethon/diagnostics/alert_rules.hpp"
#include "aethon/diagnostics/incident_report.hpp"
#include "aethon/diagnostics/metrics.hpp"
#include "aethon/diagnostics/slo_window.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

enum class ReportFormat {
    text,
    json,
};

enum class ReportSectionKind {
    summary,
    metrics,
    alerts,
    slo,
    findings,
    actions,
    timeline,
};

struct ReportField {
    std::string name;
    std::string value;
};

struct ReportRow {
    std::vector<ReportField> fields;
};

struct ReportSection {
    ReportSectionKind kind = ReportSectionKind::summary;
    std::string title;
    std::vector<std::string> columns;
    std::vector<ReportRow> rows;
    std::string note;
};

struct ReportProjection {
    std::vector<ReportSectionKind> sections;
    std::vector<std::string> metric_prefixes;
    std::vector<AlertSeverity> alert_severities;
    bool only_firing_alerts = false;
    bool include_empty_sections = false;
    std::size_t finding_limit = 0;
};

struct ReportEnvelope {
    std::string title;
    std::uint64_t generated_at_ns = 0;
    std::vector<ReportSection> sections;
    std::vector<std::string> warnings;
};

class ReportFormatter {
public:
    explicit ReportFormatter(ReportFormat format = ReportFormat::text);

    void format(ReportFormat value) noexcept;
    [[nodiscard]] ReportFormat format() const noexcept;
    [[nodiscard]] std::string render(const ReportEnvelope& envelope) const;

private:
    ReportFormat format_ = ReportFormat::text;
};

[[nodiscard]] std::string report_format_name(ReportFormat format);
[[nodiscard]] std::optional<ReportFormat> parse_report_format(std::string_view value);
[[nodiscard]] std::string report_section_kind_name(ReportSectionKind kind);
[[nodiscard]] std::optional<ReportSectionKind> parse_report_section_kind(std::string_view value);
[[nodiscard]] ReportProjection default_incident_projection();
[[nodiscard]] ReportProjection compact_incident_projection();
[[nodiscard]] ReportProjection alert_projection(bool only_firing);
[[nodiscard]] ReportEnvelope project_incident_report(const IncidentReport& report,
                                                     const ReportProjection& projection,
                                                     std::uint64_t generated_at_ns);
[[nodiscard]] ReportSection project_incident_summary(const IncidentReport& report);
[[nodiscard]] ReportSection project_metric_rollups(const std::vector<MetricRollup>& rollups,
                                                   const std::vector<std::string>& prefixes);
[[nodiscard]] ReportSection project_alert_evaluations(const std::vector<AlertEvaluation>& alerts,
                                                      const std::vector<AlertSeverity>& severities,
                                                      bool only_firing);
[[nodiscard]] ReportSection project_slo_statuses(const std::vector<SloBudgetStatus>& statuses);
[[nodiscard]] ReportSection project_incident_findings(const IncidentReport& report,
                                                     std::size_t limit);
[[nodiscard]] ReportSection project_incident_actions(const std::vector<IncidentAction>& actions);
[[nodiscard]] ReportSection project_incident_timeline(const Timeline& timeline);
[[nodiscard]] std::string render_report_envelope_text(const ReportEnvelope& envelope);
[[nodiscard]] std::string render_report_envelope_json(const ReportEnvelope& envelope);

} // namespace aethon::diagnostics
