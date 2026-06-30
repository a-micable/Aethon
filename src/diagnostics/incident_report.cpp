#include "aethon/diagnostics/incident_report.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

int incident_severity_rank(IncidentSeverity severity) {
    switch (severity) {
    case IncidentSeverity::low:
        return 0;
    case IncidentSeverity::medium:
        return 1;
    case IncidentSeverity::high:
        return 2;
    case IncidentSeverity::critical:
        return 3;
    }
    return 0;
}

TimelineLevel timeline_level_for(IncidentSeverity severity) {
    switch (severity) {
    case IncidentSeverity::low:
        return TimelineLevel::info;
    case IncidentSeverity::medium:
        return TimelineLevel::warning;
    case IncidentSeverity::high:
    case IncidentSeverity::critical:
        return TimelineLevel::error;
    }
    return TimelineLevel::info;
}

std::string labels_json(const std::vector<std::string>& labels) {
    return render_json_string_array(labels);
}

JsonObject impact_json(const IncidentImpact& impact) {
    JsonObject object;
    object.string("component", impact.component);
    object.string("region", impact.region);
    object.string("description", impact.description);
    object.number("affected_devices", impact.affected_devices);
    object.number("dropped_packets", impact.dropped_packets);
    object.number("delayed_packets", impact.delayed_packets);
    return object;
}

JsonObject finding_json(const IncidentFinding& finding) {
    JsonObject object;
    object.string("kind", finding_kind_name(finding.kind));
    object.string("severity", incident_severity_name(finding.severity));
    object.number("time_ns", finding.time_ns);
    object.string("source", finding.source);
    object.string("summary", finding.summary);
    object.string("detail", finding.detail);
    object.raw("labels", labels_json(finding.labels));
    return object;
}

JsonObject action_json(const IncidentAction& action) {
    JsonObject object;
    object.string("owner", action.owner);
    object.string("action", action.action);
    object.string("rationale", action.rationale);
    object.boolean("completed", action.completed);
    return object;
}

JsonObject timeline_event_json(const TimelineEvent& event) {
    JsonObject object;
    object.number("time_ns", event.time_ns);
    object.string("level", timeline_level_name(event.level));
    object.string("source", event.source);
    object.string("message", event.message);
    return object;
}

std::string impacts_json(const std::vector<IncidentImpact>& impacts) {
    std::vector<JsonObject> objects;
    objects.reserve(impacts.size());
    for (const auto& impact : impacts) {
        objects.push_back(impact_json(impact));
    }
    return render_json_array(objects);
}

std::string findings_json(const std::vector<IncidentFinding>& findings) {
    std::vector<JsonObject> objects;
    objects.reserve(findings.size());
    for (const auto& finding : findings) {
        objects.push_back(finding_json(finding));
    }
    return render_json_array(objects);
}

std::string actions_json(const std::vector<IncidentAction>& actions) {
    std::vector<JsonObject> objects;
    objects.reserve(actions.size());
    for (const auto& action : actions) {
        objects.push_back(action_json(action));
    }
    return render_json_array(objects);
}

std::string timeline_json(const Timeline& timeline) {
    std::vector<JsonObject> objects;
    objects.reserve(timeline.events().size());
    for (const auto& event : timeline.events()) {
        objects.push_back(timeline_event_json(event));
    }
    return render_json_array(objects);
}

std::string alert_summary_json(const std::vector<AlertEvaluation>& alerts) {
    return render_alert_evaluations_json(alerts);
}

std::string slo_summary_json(const std::vector<SloBudgetStatus>& statuses) {
    std::vector<JsonObject> objects;
    objects.reserve(statuses.size());
    for (const auto& status : statuses) {
        JsonObject object;
        object.string("id", status.objective.id);
        object.string("name", status.objective.name);
        object.string("status", slo_status_name(status.status));
        object.raw("success_ratio", std::to_string(status.success_ratio));
        object.raw("budget_used_ratio", std::to_string(status.budget_used_ratio));
        object.raw("burn_rate", std::to_string(status.burn_rate));
        objects.push_back(std::move(object));
    }
    return render_json_array(objects);
}

std::string rollups_json(const std::vector<MetricRollup>& rollups) {
    std::vector<JsonObject> objects;
    objects.reserve(rollups.size());
    for (const auto& rollup : rollups) {
        JsonObject object;
        object.string("metric", canonical_metric_key(rollup.identity));
        object.string("kind", metric_kind_name(rollup.kind));
        object.number("sample_count", static_cast<std::uint64_t>(rollup.sample_count));
        object.raw("min", std::to_string(rollup.min));
        object.raw("max", std::to_string(rollup.max));
        object.raw("average", std::to_string(rollup.average));
        object.raw("latest", std::to_string(rollup.latest));
        object.raw("delta", std::to_string(rollup.delta));
        objects.push_back(std::move(object));
    }
    return render_json_array(objects);
}

std::string finding_source_for_alert(const AlertEvaluation& alert) {
    return "alert:" + alert.rule_id;
}

std::string finding_detail_for_alert(const AlertEvaluation& alert) {
    std::ostringstream detail;
    detail << alert.reason;
    for (const auto& evidence : alert.evidence) {
        detail << "; " << evidence.metric_name;
        if (evidence.observed_value) {
            detail << "=" << *evidence.observed_value;
        }
        if (!evidence.message.empty()) {
            detail << " (" << evidence.message << ")";
        }
    }
    return detail.str();
}

IncidentAction make_action(std::string owner, std::string action, std::string rationale) {
    return IncidentAction{
        std::move(owner),
        std::move(action),
        std::move(rationale),
        false,
    };
}

} // namespace

IncidentReportBuilder& IncidentReportBuilder::id(std::string value) {
    report_.id = std::move(value);
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::title(std::string value) {
    report_.title = std::move(value);
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::severity(IncidentSeverity value) {
    report_.severity = value;
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::status(IncidentStatus value) {
    report_.status = value;
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::opened_at(std::uint64_t value) {
    report_.opened_at_ns = value;
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::updated_at(std::uint64_t value) {
    report_.updated_at_ns = value;
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::resolved_at(std::uint64_t value) {
    report_.resolved_at_ns = value;
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::summary(std::string value) {
    report_.summary = std::move(value);
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::impact(IncidentImpact value) {
    report_.impacts.push_back(std::move(value));
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::finding(IncidentFinding value) {
    report_.findings.push_back(std::move(value));
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::action(IncidentAction value) {
    report_.actions.push_back(std::move(value));
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::alerts(std::vector<AlertEvaluation> values) {
    report_.alerts = std::move(values);
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::slo_status(SloBudgetStatus value) {
    report_.slo_statuses.push_back(std::move(value));
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::metric_rollups(std::vector<MetricRollup> values) {
    report_.metric_rollups = std::move(values);
    return *this;
}

IncidentReportBuilder& IncidentReportBuilder::timeline(Timeline value) {
    report_.timeline = std::move(value);
    return *this;
}

IncidentReport IncidentReportBuilder::build() const {
    return report_;
}

void IncidentReportBuilder::reset() {
    report_ = IncidentReport{};
}

std::string incident_severity_name(IncidentSeverity severity) {
    switch (severity) {
    case IncidentSeverity::low:
        return "low";
    case IncidentSeverity::medium:
        return "medium";
    case IncidentSeverity::high:
        return "high";
    case IncidentSeverity::critical:
        return "critical";
    }
    return "unknown";
}

std::optional<IncidentSeverity> parse_incident_severity(std::string_view value) {
    if (value == "low") {
        return IncidentSeverity::low;
    }
    if (value == "medium") {
        return IncidentSeverity::medium;
    }
    if (value == "high") {
        return IncidentSeverity::high;
    }
    if (value == "critical") {
        return IncidentSeverity::critical;
    }
    return std::nullopt;
}

std::string incident_status_name(IncidentStatus status) {
    switch (status) {
    case IncidentStatus::investigating:
        return "investigating";
    case IncidentStatus::identified:
        return "identified";
    case IncidentStatus::mitigating:
        return "mitigating";
    case IncidentStatus::resolved:
        return "resolved";
    }
    return "unknown";
}

std::optional<IncidentStatus> parse_incident_status(std::string_view value) {
    if (value == "investigating") {
        return IncidentStatus::investigating;
    }
    if (value == "identified") {
        return IncidentStatus::identified;
    }
    if (value == "mitigating") {
        return IncidentStatus::mitigating;
    }
    if (value == "resolved") {
        return IncidentStatus::resolved;
    }
    return std::nullopt;
}

std::string finding_kind_name(FindingKind kind) {
    switch (kind) {
    case FindingKind::metric_anomaly:
        return "metric_anomaly";
    case FindingKind::alert:
        return "alert";
    case FindingKind::slo_burn:
        return "slo_burn";
    case FindingKind::timeline_event:
        return "timeline_event";
    case FindingKind::operator_note:
        return "operator_note";
    }
    return "unknown";
}

IncidentSeverity severity_from_alert(AlertSeverity severity) {
    switch (severity) {
    case AlertSeverity::info:
        return IncidentSeverity::low;
    case AlertSeverity::warning:
        return IncidentSeverity::medium;
    case AlertSeverity::critical:
        return IncidentSeverity::high;
    }
    return IncidentSeverity::medium;
}

IncidentSeverity severity_from_slo(SloStatus status) {
    switch (status) {
    case SloStatus::unknown:
        return IncidentSeverity::low;
    case SloStatus::healthy:
        return IncidentSeverity::low;
    case SloStatus::warning:
        return IncidentSeverity::medium;
    case SloStatus::exhausted:
        return IncidentSeverity::critical;
    }
    return IncidentSeverity::medium;
}

IncidentReportSummary summarize_incident_report(const IncidentReport& report) {
    IncidentReportSummary summary;
    summary.id = report.id;
    summary.severity = report.severity;
    summary.status = report.status;
    summary.opened_at_ns = report.opened_at_ns;
    summary.updated_at_ns = report.updated_at_ns;
    const auto end_time = report.resolved_at_ns.value_or(report.updated_at_ns);
    summary.duration_ns = end_time >= report.opened_at_ns ? end_time - report.opened_at_ns : 0;
    summary.impact_count = report.impacts.size();
    summary.finding_count = report.findings.size();
    summary.action_count = report.actions.size();
    for (const auto& action : report.actions) {
        if (!action.completed) {
            ++summary.open_action_count;
        }
    }
    for (const auto& alert : report.alerts) {
        if (alert.firing()) {
            ++summary.firing_alert_count;
        }
    }
    for (const auto& slo : report.slo_statuses) {
        if (slo.status == SloStatus::exhausted) {
            ++summary.exhausted_slo_count;
        }
    }
    return summary;
}

std::vector<IncidentFinding> findings_from_alerts(const std::vector<AlertEvaluation>& alerts) {
    std::vector<IncidentFinding> findings;
    for (const auto& alert : alerts) {
        if (!alert.firing()) {
            continue;
        }
        findings.push_back(IncidentFinding{
            FindingKind::alert,
            severity_from_alert(alert.severity),
            alert.evaluated_at_ns,
            finding_source_for_alert(alert),
            alert.title,
            finding_detail_for_alert(alert),
            alert.labels,
        });
    }
    return findings;
}

std::vector<IncidentFinding> findings_from_slos(const std::vector<SloBudgetStatus>& statuses) {
    std::vector<IncidentFinding> findings;
    for (const auto& status : statuses) {
        if (status.status != SloStatus::warning && status.status != SloStatus::exhausted) {
            continue;
        }
        std::ostringstream detail;
        detail << "success_ratio=" << status.success_ratio
               << " budget_used=" << status.budget_used_ratio
               << " burn_rate=" << status.burn_rate;
        findings.push_back(IncidentFinding{
            FindingKind::slo_burn,
            severity_from_slo(status.status),
            status.evaluated_at_ns,
            "slo:" + status.objective.id,
            status.objective.name + " is " + slo_status_name(status.status),
            detail.str(),
            {"slo"},
        });
    }
    return findings;
}

std::vector<IncidentFinding> findings_from_metric_deltas(const std::vector<MetricDelta>& deltas,
                                                         double relative_threshold) {
    std::vector<IncidentFinding> findings;
    for (const auto& delta : deltas) {
        if (!delta.previous || !delta.current) {
            continue;
        }
        if (std::abs(delta.relative_change) < relative_threshold) {
            continue;
        }
        std::ostringstream detail;
        detail << "previous=" << *delta.previous
               << " current=" << *delta.current
               << " relative_change=" << delta.relative_change;
        findings.push_back(IncidentFinding{
            FindingKind::metric_anomaly,
            std::abs(delta.relative_change) >= relative_threshold * 2.0
                ? IncidentSeverity::high
                : IncidentSeverity::medium,
            0,
            "metric:" + canonical_metric_key(delta.identity),
            canonical_metric_key(delta.identity) + " changed " + metric_trend_name(delta.trend),
            detail.str(),
            {"metric"},
        });
    }
    return findings;
}

std::vector<IncidentAction> recommend_incident_actions(const IncidentReport& report) {
    std::vector<IncidentAction> actions;
    auto summary = summarize_incident_report(report);
    if (summary.exhausted_slo_count > 0) {
        actions.push_back(make_action(
            "oncall",
            "freeze risky deploys for affected components",
            "one or more SLO error budgets are exhausted"));
    }
    if (summary.firing_alert_count > 0) {
        actions.push_back(make_action(
            "oncall",
            "inspect firing alert evidence and correlate with timeline",
            "active alerts identify the highest-signal failure modes"));
    }
    if (summary.open_action_count == 0 && report.status != IncidentStatus::resolved) {
        actions.push_back(make_action(
            "incident-commander",
            "assign owners for mitigation and customer impact",
            "unresolved incidents should have explicit next actions"));
    }
    if (report.impacts.empty()) {
        actions.push_back(make_action(
            "support",
            "confirm customer and device impact",
            "impact fields are empty and reporting may be incomplete"));
    }
    return actions;
}

Timeline build_incident_timeline(const IncidentReport& report) {
    Timeline timeline = report.timeline;
    timeline.add(
        report.opened_at_ns,
        TimelineLevel::info,
        "incident",
        "opened " + report.id);
    if (report.resolved_at_ns) {
        timeline.add(
            *report.resolved_at_ns,
            TimelineLevel::info,
            "incident",
            "resolved " + report.id);
    }
    for (const auto& finding : report.findings) {
        timeline.add(
            finding.time_ns,
            timeline_level_for(finding.severity),
            finding.source,
            finding.summary);
    }
    for (const auto& alert : report.alerts) {
        if (alert.firing()) {
            timeline.add(
                alert.evaluated_at_ns,
                timeline_level_for(severity_from_alert(alert.severity)),
                "alert:" + alert.rule_id,
                alert.reason);
        }
    }
    timeline.sort();
    return timeline;
}

std::vector<IncidentFinding> top_incident_findings(const IncidentReport& report, std::size_t limit) {
    auto findings = report.findings;
    std::sort(
        findings.begin(),
        findings.end(),
        [](const IncidentFinding& left, const IncidentFinding& right) {
            if (incident_severity_rank(left.severity) != incident_severity_rank(right.severity)) {
                return incident_severity_rank(left.severity) > incident_severity_rank(right.severity);
            }
            return left.time_ns < right.time_ns;
        });
    if (limit != 0 && findings.size() > limit) {
        findings.resize(limit);
    }
    return findings;
}

std::string render_incident_summary_text(const IncidentReportSummary& summary) {
    TextTable table;
    table.headers = {"incident", "severity", "status", "duration_ns", "impacts", "findings", "open_actions", "alerts", "slos"};
    add_row(
        table,
        {
            summary.id,
            incident_severity_name(summary.severity),
            incident_status_name(summary.status),
            std::to_string(summary.duration_ns),
            std::to_string(summary.impact_count),
            std::to_string(summary.finding_count),
            std::to_string(summary.open_action_count),
            std::to_string(summary.firing_alert_count),
            std::to_string(summary.exhausted_slo_count),
        });
    return render_text_table(table);
}

std::string render_incident_report_text(const IncidentReport& report) {
    std::ostringstream out;
    out << "incident_report\n";
    out << render_incident_summary_text(summarize_incident_report(report));
    out << "summary: " << report.summary << "\n";

    TextTable impacts;
    impacts.headers = {"component", "region", "affected", "dropped", "delayed", "description"};
    for (const auto& impact : report.impacts) {
        add_row(
            impacts,
            {
                impact.component,
                impact.region,
                std::to_string(impact.affected_devices),
                std::to_string(impact.dropped_packets),
                std::to_string(impact.delayed_packets),
                impact.description,
            });
    }
    out << "impacts\n" << render_text_table(impacts);

    TextTable findings;
    findings.headers = {"severity", "kind", "time_ns", "source", "summary"};
    for (const auto& finding : top_incident_findings(report, 0)) {
        add_row(
            findings,
            {
                incident_severity_name(finding.severity),
                finding_kind_name(finding.kind),
                std::to_string(finding.time_ns),
                finding.source,
                finding.summary,
            });
    }
    out << "findings\n" << render_text_table(findings);

    TextTable actions;
    actions.headers = {"owner", "completed", "action", "rationale"};
    for (const auto& action : report.actions) {
        add_row(
            actions,
            {
                action.owner,
                action.completed ? "yes" : "no",
                action.action,
                action.rationale,
            });
    }
    out << "actions\n" << render_text_table(actions);

    if (!report.alerts.empty()) {
        out << "alerts\n" << render_alert_evaluations_text(report.alerts);
    }
    if (!report.slo_statuses.empty()) {
        for (const auto& slo : report.slo_statuses) {
            out << "slo\n" << render_slo_budget_text(slo);
        }
    }
    if (!report.metric_rollups.empty()) {
        out << "metric_rollups\n" << render_metric_rollups_text(report.metric_rollups);
    }
    out << render_timeline(build_incident_timeline(report));
    return out.str();
}

std::string render_incident_report_json(const IncidentReport& report) {
    auto summary = summarize_incident_report(report);
    JsonObject summary_object;
    summary_object.string("id", summary.id);
    summary_object.string("severity", incident_severity_name(summary.severity));
    summary_object.string("status", incident_status_name(summary.status));
    summary_object.number("opened_at_ns", summary.opened_at_ns);
    summary_object.number("updated_at_ns", summary.updated_at_ns);
    summary_object.number("duration_ns", summary.duration_ns);
    summary_object.number("impact_count", static_cast<std::uint64_t>(summary.impact_count));
    summary_object.number("finding_count", static_cast<std::uint64_t>(summary.finding_count));
    summary_object.number("action_count", static_cast<std::uint64_t>(summary.action_count));
    summary_object.number("open_action_count", static_cast<std::uint64_t>(summary.open_action_count));
    summary_object.number("firing_alert_count", static_cast<std::uint64_t>(summary.firing_alert_count));
    summary_object.number("exhausted_slo_count", static_cast<std::uint64_t>(summary.exhausted_slo_count));

    JsonObject root;
    root.string("id", report.id);
    root.string("title", report.title);
    root.string("summary_text", report.summary);
    root.raw("summary", render_json_object(summary_object));
    root.raw("impacts", impacts_json(report.impacts));
    root.raw("findings", findings_json(report.findings));
    root.raw("actions", actions_json(report.actions));
    root.raw("alerts", alert_summary_json(report.alerts));
    root.raw("slo_statuses", slo_summary_json(report.slo_statuses));
    root.raw("metric_rollups", rollups_json(report.metric_rollups));
    root.raw("timeline", timeline_json(build_incident_timeline(report)));
    return render_json_object(root);
}

} // namespace aethon::diagnostics
