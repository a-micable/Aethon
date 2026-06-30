#pragma once

#include "aethon/diagnostics/alert_rules.hpp"
#include "aethon/diagnostics/metrics.hpp"
#include "aethon/diagnostics/slo_window.hpp"
#include "aethon/diagnostics/timeline.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

enum class IncidentSeverity {
    low,
    medium,
    high,
    critical,
};

enum class IncidentStatus {
    investigating,
    identified,
    mitigating,
    resolved,
};

enum class FindingKind {
    metric_anomaly,
    alert,
    slo_burn,
    timeline_event,
    operator_note,
};

struct IncidentImpact {
    std::string component;
    std::string region;
    std::string description;
    std::uint64_t affected_devices = 0;
    std::uint64_t dropped_packets = 0;
    std::uint64_t delayed_packets = 0;
};

struct IncidentFinding {
    FindingKind kind = FindingKind::operator_note;
    IncidentSeverity severity = IncidentSeverity::medium;
    std::uint64_t time_ns = 0;
    std::string source;
    std::string summary;
    std::string detail;
    std::vector<std::string> labels;
};

struct IncidentAction {
    std::string owner;
    std::string action;
    std::string rationale;
    bool completed = false;
};

struct IncidentReport {
    std::string id;
    std::string title;
    IncidentSeverity severity = IncidentSeverity::medium;
    IncidentStatus status = IncidentStatus::investigating;
    std::uint64_t opened_at_ns = 0;
    std::uint64_t updated_at_ns = 0;
    std::optional<std::uint64_t> resolved_at_ns;
    std::string summary;
    std::vector<IncidentImpact> impacts;
    std::vector<IncidentFinding> findings;
    std::vector<IncidentAction> actions;
    std::vector<AlertEvaluation> alerts;
    std::vector<SloBudgetStatus> slo_statuses;
    std::vector<MetricRollup> metric_rollups;
    Timeline timeline;
};

struct IncidentReportSummary {
    std::string id;
    IncidentSeverity severity = IncidentSeverity::medium;
    IncidentStatus status = IncidentStatus::investigating;
    std::uint64_t opened_at_ns = 0;
    std::uint64_t updated_at_ns = 0;
    std::uint64_t duration_ns = 0;
    std::size_t impact_count = 0;
    std::size_t finding_count = 0;
    std::size_t action_count = 0;
    std::size_t open_action_count = 0;
    std::size_t firing_alert_count = 0;
    std::size_t exhausted_slo_count = 0;
};

class IncidentReportBuilder {
public:
    IncidentReportBuilder& id(std::string value);
    IncidentReportBuilder& title(std::string value);
    IncidentReportBuilder& severity(IncidentSeverity value);
    IncidentReportBuilder& status(IncidentStatus value);
    IncidentReportBuilder& opened_at(std::uint64_t value);
    IncidentReportBuilder& updated_at(std::uint64_t value);
    IncidentReportBuilder& resolved_at(std::uint64_t value);
    IncidentReportBuilder& summary(std::string value);
    IncidentReportBuilder& impact(IncidentImpact value);
    IncidentReportBuilder& finding(IncidentFinding value);
    IncidentReportBuilder& action(IncidentAction value);
    IncidentReportBuilder& alerts(std::vector<AlertEvaluation> values);
    IncidentReportBuilder& slo_status(SloBudgetStatus value);
    IncidentReportBuilder& metric_rollups(std::vector<MetricRollup> values);
    IncidentReportBuilder& timeline(Timeline value);

    [[nodiscard]] IncidentReport build() const;
    void reset();

private:
    IncidentReport report_;
};

[[nodiscard]] std::string incident_severity_name(IncidentSeverity severity);
[[nodiscard]] std::optional<IncidentSeverity> parse_incident_severity(std::string_view value);
[[nodiscard]] std::string incident_status_name(IncidentStatus status);
[[nodiscard]] std::optional<IncidentStatus> parse_incident_status(std::string_view value);
[[nodiscard]] std::string finding_kind_name(FindingKind kind);
[[nodiscard]] IncidentSeverity severity_from_alert(AlertSeverity severity);
[[nodiscard]] IncidentSeverity severity_from_slo(SloStatus status);
[[nodiscard]] IncidentReportSummary summarize_incident_report(const IncidentReport& report);
[[nodiscard]] std::vector<IncidentFinding> findings_from_alerts(const std::vector<AlertEvaluation>& alerts);
[[nodiscard]] std::vector<IncidentFinding> findings_from_slos(const std::vector<SloBudgetStatus>& statuses);
[[nodiscard]] std::vector<IncidentFinding> findings_from_metric_deltas(const std::vector<MetricDelta>& deltas,
                                                                       double relative_threshold);
[[nodiscard]] std::vector<IncidentAction> recommend_incident_actions(const IncidentReport& report);
[[nodiscard]] Timeline build_incident_timeline(const IncidentReport& report);
[[nodiscard]] std::vector<IncidentFinding> top_incident_findings(const IncidentReport& report,
                                                                std::size_t limit);
[[nodiscard]] std::string render_incident_summary_text(const IncidentReportSummary& summary);
[[nodiscard]] std::string render_incident_report_text(const IncidentReport& report);
[[nodiscard]] std::string render_incident_report_json(const IncidentReport& report);

} // namespace aethon::diagnostics
