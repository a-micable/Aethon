#include "aethon/diagnostics/report_formatter.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

bool has_section(const ReportProjection& projection, ReportSectionKind kind) {
    return std::find(projection.sections.begin(), projection.sections.end(), kind) != projection.sections.end();
}

bool severity_allowed(AlertSeverity severity, const std::vector<AlertSeverity>& severities) {
    return severities.empty()
        || std::find(severities.begin(), severities.end(), severity) != severities.end();
}

bool metric_allowed(const MetricRollup& rollup, const std::vector<std::string>& prefixes) {
    if (prefixes.empty()) {
        return true;
    }
    const auto key = canonical_metric_key(rollup.identity);
    for (const auto& prefix : prefixes) {
        if (key.rfind(prefix, 0) == 0 || rollup.identity.name.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

void add_field(ReportRow& row, std::string name, std::string value) {
    row.fields.push_back(ReportField{
        std::move(name),
        std::move(value),
    });
}

std::string field_value(const ReportRow& row, std::string_view name) {
    for (const auto& field : row.fields) {
        if (field.name == name) {
            return field.value;
        }
    }
    return "";
}

JsonObject row_json(const ReportRow& row) {
    JsonObject object;
    for (const auto& field : row.fields) {
        object.string(field.name, field.value);
    }
    return object;
}

std::string rows_json(const std::vector<ReportRow>& rows) {
    std::vector<JsonObject> objects;
    objects.reserve(rows.size());
    for (const auto& row : rows) {
        objects.push_back(row_json(row));
    }
    return render_json_array(objects);
}

JsonObject section_json(const ReportSection& section) {
    JsonObject object;
    object.string("kind", report_section_kind_name(section.kind));
    object.string("title", section.title);
    object.raw("columns", render_json_string_array(section.columns));
    object.raw("rows", rows_json(section.rows));
    object.string("note", section.note);
    return object;
}

std::string sections_json(const std::vector<ReportSection>& sections) {
    std::vector<JsonObject> objects;
    objects.reserve(sections.size());
    for (const auto& section : sections) {
        objects.push_back(section_json(section));
    }
    return render_json_array(objects);
}

TextTable section_table(const ReportSection& section) {
    TextTable table;
    table.headers = section.columns;
    for (const auto& row : section.rows) {
        std::vector<std::string> cells;
        cells.reserve(section.columns.size());
        for (const auto& column : section.columns) {
            cells.push_back(field_value(row, column));
        }
        add_row(table, std::move(cells));
    }
    return table;
}

std::string format_ratio(double value) {
    return std::to_string(value);
}

std::string format_bool(bool value) {
    return value ? "yes" : "no";
}

} // namespace

ReportFormatter::ReportFormatter(ReportFormat format)
    : format_(format) {
}

void ReportFormatter::format(ReportFormat value) noexcept {
    format_ = value;
}

ReportFormat ReportFormatter::format() const noexcept {
    return format_;
}

std::string ReportFormatter::render(const ReportEnvelope& envelope) const {
    if (format_ == ReportFormat::json) {
        return render_report_envelope_json(envelope);
    }
    return render_report_envelope_text(envelope);
}

std::string report_format_name(ReportFormat format) {
    switch (format) {
    case ReportFormat::text:
        return "text";
    case ReportFormat::json:
        return "json";
    }
    return "unknown";
}

std::optional<ReportFormat> parse_report_format(std::string_view value) {
    if (value == "text") {
        return ReportFormat::text;
    }
    if (value == "json") {
        return ReportFormat::json;
    }
    return std::nullopt;
}

std::string report_section_kind_name(ReportSectionKind kind) {
    switch (kind) {
    case ReportSectionKind::summary:
        return "summary";
    case ReportSectionKind::metrics:
        return "metrics";
    case ReportSectionKind::alerts:
        return "alerts";
    case ReportSectionKind::slo:
        return "slo";
    case ReportSectionKind::findings:
        return "findings";
    case ReportSectionKind::actions:
        return "actions";
    case ReportSectionKind::timeline:
        return "timeline";
    }
    return "unknown";
}

std::optional<ReportSectionKind> parse_report_section_kind(std::string_view value) {
    if (value == "summary") {
        return ReportSectionKind::summary;
    }
    if (value == "metrics") {
        return ReportSectionKind::metrics;
    }
    if (value == "alerts") {
        return ReportSectionKind::alerts;
    }
    if (value == "slo") {
        return ReportSectionKind::slo;
    }
    if (value == "findings") {
        return ReportSectionKind::findings;
    }
    if (value == "actions") {
        return ReportSectionKind::actions;
    }
    if (value == "timeline") {
        return ReportSectionKind::timeline;
    }
    return std::nullopt;
}

ReportProjection default_incident_projection() {
    ReportProjection projection;
    projection.sections = {
        ReportSectionKind::summary,
        ReportSectionKind::alerts,
        ReportSectionKind::slo,
        ReportSectionKind::findings,
        ReportSectionKind::actions,
        ReportSectionKind::metrics,
        ReportSectionKind::timeline,
    };
    projection.include_empty_sections = true;
    return projection;
}

ReportProjection compact_incident_projection() {
    ReportProjection projection;
    projection.sections = {
        ReportSectionKind::summary,
        ReportSectionKind::alerts,
        ReportSectionKind::slo,
        ReportSectionKind::findings,
    };
    projection.only_firing_alerts = true;
    projection.finding_limit = 5;
    return projection;
}

ReportProjection alert_projection(bool only_firing) {
    ReportProjection projection;
    projection.sections = {
        ReportSectionKind::summary,
        ReportSectionKind::alerts,
    };
    projection.only_firing_alerts = only_firing;
    projection.include_empty_sections = true;
    return projection;
}

ReportEnvelope project_incident_report(const IncidentReport& report,
                                       const ReportProjection& projection,
                                       std::uint64_t generated_at_ns) {
    ReportEnvelope envelope;
    envelope.title = report.title.empty() ? report.id : report.title;
    envelope.generated_at_ns = generated_at_ns;

    auto maybe_add = [&](ReportSection section) {
        if (!section.rows.empty() || projection.include_empty_sections) {
            envelope.sections.push_back(std::move(section));
        }
    };

    if (has_section(projection, ReportSectionKind::summary)) {
        maybe_add(project_incident_summary(report));
    }
    if (has_section(projection, ReportSectionKind::metrics)) {
        maybe_add(project_metric_rollups(report.metric_rollups, projection.metric_prefixes));
    }
    if (has_section(projection, ReportSectionKind::alerts)) {
        maybe_add(project_alert_evaluations(
            report.alerts,
            projection.alert_severities,
            projection.only_firing_alerts));
    }
    if (has_section(projection, ReportSectionKind::slo)) {
        maybe_add(project_slo_statuses(report.slo_statuses));
    }
    if (has_section(projection, ReportSectionKind::findings)) {
        maybe_add(project_incident_findings(report, projection.finding_limit));
    }
    if (has_section(projection, ReportSectionKind::actions)) {
        maybe_add(project_incident_actions(report.actions));
    }
    if (has_section(projection, ReportSectionKind::timeline)) {
        maybe_add(project_incident_timeline(build_incident_timeline(report)));
    }
    if (envelope.sections.empty()) {
        envelope.warnings.push_back("projection selected no report sections");
    }
    return envelope;
}

ReportSection project_incident_summary(const IncidentReport& report) {
    auto summary = summarize_incident_report(report);
    ReportSection section;
    section.kind = ReportSectionKind::summary;
    section.title = "Incident Summary";
    section.columns = {"incident", "severity", "status", "duration_ns", "findings", "alerts", "slos"};
    ReportRow row;
    add_field(row, "incident", summary.id);
    add_field(row, "severity", incident_severity_name(summary.severity));
    add_field(row, "status", incident_status_name(summary.status));
    add_field(row, "duration_ns", std::to_string(summary.duration_ns));
    add_field(row, "findings", std::to_string(summary.finding_count));
    add_field(row, "alerts", std::to_string(summary.firing_alert_count));
    add_field(row, "slos", std::to_string(summary.exhausted_slo_count));
    section.rows.push_back(std::move(row));
    return section;
}

ReportSection project_metric_rollups(const std::vector<MetricRollup>& rollups,
                                     const std::vector<std::string>& prefixes) {
    ReportSection section;
    section.kind = ReportSectionKind::metrics;
    section.title = "Metric Rollups";
    section.columns = {"metric", "kind", "samples", "average", "latest", "trend"};
    for (const auto& rollup : rollups) {
        if (!metric_allowed(rollup, prefixes)) {
            continue;
        }
        ReportRow row;
        add_field(row, "metric", canonical_metric_key(rollup.identity));
        add_field(row, "kind", metric_kind_name(rollup.kind));
        add_field(row, "samples", std::to_string(rollup.sample_count));
        add_field(row, "average", format_metric_value(rollup.average, rollup.unit));
        add_field(row, "latest", format_metric_value(rollup.latest, rollup.unit));
        add_field(row, "trend", rollup.delta > 0.0 ? "rising" : (rollup.delta < 0.0 ? "falling" : "flat"));
        section.rows.push_back(std::move(row));
    }
    return section;
}

ReportSection project_alert_evaluations(const std::vector<AlertEvaluation>& alerts,
                                        const std::vector<AlertSeverity>& severities,
                                        bool only_firing) {
    ReportSection section;
    section.kind = ReportSectionKind::alerts;
    section.title = "Alert Evaluations";
    section.columns = {"rule", "severity", "state", "reason", "evidence"};
    for (const auto& alert : alerts) {
        if (only_firing && !alert.firing()) {
            continue;
        }
        if (!severity_allowed(alert.severity, severities)) {
            continue;
        }
        ReportRow row;
        add_field(row, "rule", alert.rule_id);
        add_field(row, "severity", alert_severity_name(alert.severity));
        add_field(row, "state", alert_state_name(alert.state));
        add_field(row, "reason", alert.reason);
        add_field(row, "evidence", std::to_string(alert.evidence.size()));
        section.rows.push_back(std::move(row));
    }
    return section;
}

ReportSection project_slo_statuses(const std::vector<SloBudgetStatus>& statuses) {
    ReportSection section;
    section.kind = ReportSectionKind::slo;
    section.title = "SLO Status";
    section.columns = {"slo", "status", "success", "budget_used", "burn_rate", "bad_events"};
    for (const auto& status : statuses) {
        ReportRow row;
        add_field(row, "slo", status.objective.id);
        add_field(row, "status", slo_status_name(status.status));
        add_field(row, "success", format_ratio(status.success_ratio));
        add_field(row, "budget_used", format_ratio(status.budget_used_ratio));
        add_field(row, "burn_rate", format_ratio(status.burn_rate));
        add_field(row, "bad_events", std::to_string(status.bad_events));
        section.rows.push_back(std::move(row));
    }
    return section;
}

ReportSection project_incident_findings(const IncidentReport& report, std::size_t limit) {
    ReportSection section;
    section.kind = ReportSectionKind::findings;
    section.title = "Top Findings";
    section.columns = {"severity", "kind", "time_ns", "source", "summary"};
    for (const auto& finding : top_incident_findings(report, limit)) {
        ReportRow row;
        add_field(row, "severity", incident_severity_name(finding.severity));
        add_field(row, "kind", finding_kind_name(finding.kind));
        add_field(row, "time_ns", std::to_string(finding.time_ns));
        add_field(row, "source", finding.source);
        add_field(row, "summary", finding.summary);
        section.rows.push_back(std::move(row));
    }
    return section;
}

ReportSection project_incident_actions(const std::vector<IncidentAction>& actions) {
    ReportSection section;
    section.kind = ReportSectionKind::actions;
    section.title = "Incident Actions";
    section.columns = {"owner", "completed", "action", "rationale"};
    for (const auto& action : actions) {
        ReportRow row;
        add_field(row, "owner", action.owner);
        add_field(row, "completed", format_bool(action.completed));
        add_field(row, "action", action.action);
        add_field(row, "rationale", action.rationale);
        section.rows.push_back(std::move(row));
    }
    return section;
}

ReportSection project_incident_timeline(const Timeline& timeline) {
    ReportSection section;
    section.kind = ReportSectionKind::timeline;
    section.title = "Incident Timeline";
    section.columns = {"time_ns", "level", "source", "message"};
    for (const auto& event : timeline.events()) {
        ReportRow row;
        add_field(row, "time_ns", std::to_string(event.time_ns));
        add_field(row, "level", timeline_level_name(event.level));
        add_field(row, "source", event.source);
        add_field(row, "message", event.message);
        section.rows.push_back(std::move(row));
    }
    return section;
}

std::string render_report_envelope_text(const ReportEnvelope& envelope) {
    std::ostringstream out;
    out << "report: " << envelope.title << "\n";
    out << "generated_at_ns: " << envelope.generated_at_ns << "\n";
    for (const auto& warning : envelope.warnings) {
        out << "warning: " << warning << "\n";
    }
    for (const auto& section : envelope.sections) {
        out << "\n" << section.title << "\n";
        if (!section.note.empty()) {
            out << section.note << "\n";
        }
        out << render_text_table(section_table(section));
    }
    return out.str();
}

std::string render_report_envelope_json(const ReportEnvelope& envelope) {
    JsonObject root;
    root.string("title", envelope.title);
    root.number("generated_at_ns", envelope.generated_at_ns);
    root.raw("warnings", render_json_string_array(envelope.warnings));
    root.raw("sections", sections_json(envelope.sections));
    return render_json_object(root);
}

} // namespace aethon::diagnostics
