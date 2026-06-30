#include "test_harness.hpp"

#include "aethon/diagnostics/alert_rules.hpp"
#include "aethon/diagnostics/incident_report.hpp"
#include "aethon/diagnostics/metrics.hpp"
#include "aethon/diagnostics/report_formatter.hpp"
#include "aethon/diagnostics/slo_window.hpp"

#include <string>

AETHON_TEST(metric_snapshot_rollups_and_latency_histogram_render) {
    aethon::diagnostics::MetricSnapshotBuilder builder;
    auto snapshot = builder.capture_time(100)
        .counter("packets.ingested", 10, {{"region", "east"}})
        .gauge("queue.depth", 7, {{"region", "east"}})
        .latency("decode.latency", 1500, {{"stage", "decode"}})
        .build();

    auto by_region = snapshot.find_by_tag("region", "east");
    auto series = aethon::diagnostics::group_metric_series(snapshot.samples());
    auto rollups = aethon::diagnostics::rollup_series(series);

    aethon::diagnostics::LatencyHistogram histogram({1000, 2000, 5000});
    histogram.observe(800, 10, {{"stage", "decode"}});
    histogram.observe(1200, 20, {{"stage", "decode"}});
    histogram.observe(4000, 30, {{"stage", "write"}});
    auto stats = histogram.stats();
    auto json = aethon::diagnostics::render_latency_histogram_json(histogram);

    AETHON_REQUIRE(snapshot.size() == 3);
    AETHON_REQUIRE(by_region.size() == 2);
    AETHON_REQUIRE(rollups.size() == 3);
    AETHON_REQUIRE(stats.count == 3);
    AETHON_REQUIRE(stats.p50_ns == 1200);
    AETHON_REQUIRE(json.find("\"p95_ns\"") != std::string::npos);
}

AETHON_TEST(alert_rules_evaluate_threshold_rate_and_suppression) {
    aethon::diagnostics::MetricSnapshotBuilder builder;
    auto current = builder.capture_time(3000000000ULL)
        .gauge("queue.depth", 42, {{"region", "east"}})
        .counter("packets.dropped", 50, {{"region", "east"}})
        .build();

    std::vector<aethon::diagnostics::MetricSample> history;
    history.push_back({aethon::diagnostics::make_metric_identity("packets.dropped", {{"region", "east"}}),
                       aethon::diagnostics::MetricKind::counter,
                       10,
                       1000000000ULL,
                       "count",
                       {}});

    aethon::diagnostics::AlertRule threshold;
    threshold.id = "queue-high";
    threshold.title = "Queue depth high";
    threshold.severity = aethon::diagnostics::AlertSeverity::critical;
    threshold.condition = aethon::diagnostics::threshold_condition(
        "queue.depth",
        aethon::diagnostics::AlertComparator::greater_than,
        10,
        {{"region", "east"}});

    aethon::diagnostics::AlertRule rate;
    rate.id = "drop-rate";
    rate.title = "Packet drop rate high";
    rate.evaluation_window_ns = 3000000000ULL;
    rate.condition = aethon::diagnostics::rate_condition(
        "packets.dropped",
        aethon::diagnostics::AlertComparator::greater_or_equal,
        10.0,
        {{"region", "east"}});

    aethon::diagnostics::AlertRule suppressed = threshold;
    suppressed.id = "suppressed";
    suppressed.suppressions.push_back({"maintenance", 1, 4000000000ULL});

    aethon::diagnostics::AlertRuleSet rules;
    rules.add(threshold);
    rules.add(rate);
    rules.add(suppressed);

    auto evaluations = rules.evaluate({current, history, 3000000000ULL});
    auto summary = aethon::diagnostics::summarize_alert_evaluations(evaluations, 3000000000ULL);
    auto firing = aethon::diagnostics::firing_alerts(evaluations);

    AETHON_REQUIRE(evaluations.size() == 3);
    AETHON_REQUIRE(firing.size() == 2);
    AETHON_REQUIRE(summary.active_rules == 2);
    AETHON_REQUIRE(summary.suppressed_rules == 1);
}

AETHON_TEST(slo_window_reports_budget_and_multi_window_burn) {
    auto objective = aethon::diagnostics::make_availability_slo(
        "archive-write",
        "Archive write availability",
        0.95,
        10000000000ULL);
    aethon::diagnostics::SloWindow window(objective);
    window.good(1000000000ULL);
    window.good(2000000000ULL);
    window.bad(3000000000ULL, 1, "writer", "fsync timeout");
    window.good(4000000000ULL);

    auto status = window.evaluate(5000000000ULL);
    auto report = aethon::diagnostics::evaluate_multi_window_slo(
        window,
        5000000000ULL,
        {2000000000ULL, 5000000000ULL});
    auto text = aethon::diagnostics::render_multi_window_slo_text(report);

    AETHON_REQUIRE(status.total_weight == 4);
    AETHON_REQUIRE(status.bad_weight == 1);
    AETHON_REQUIRE(status.status == aethon::diagnostics::SloStatus::exhausted);
    AETHON_REQUIRE(report.windows.size() == 2);
    AETHON_REQUIRE(text.find("recommendation:") != std::string::npos);
}

AETHON_TEST(incident_report_collects_alert_slo_and_metric_findings) {
    aethon::diagnostics::MetricSnapshotBuilder previous_builder;
    auto previous = previous_builder.capture_time(100)
        .gauge("queue.depth", 10, {{"region", "east"}})
        .build();
    aethon::diagnostics::MetricSnapshotBuilder current_builder;
    auto current = current_builder.capture_time(200)
        .gauge("queue.depth", 40, {{"region", "east"}})
        .build();
    auto deltas = aethon::diagnostics::compare_metric_snapshots(previous, current);
    auto metric_findings = aethon::diagnostics::findings_from_metric_deltas(deltas, 1.0);

    aethon::diagnostics::AlertEvaluation alert;
    alert.rule_id = "queue-high";
    alert.title = "Queue depth high";
    alert.severity = aethon::diagnostics::AlertSeverity::critical;
    alert.state = aethon::diagnostics::AlertState::active;
    alert.evaluated_at_ns = 200;
    alert.reason = "threshold condition matched";
    auto alert_findings = aethon::diagnostics::findings_from_alerts({alert});

    auto objective = aethon::diagnostics::make_availability_slo("archive-write", "Archive write", 0.99, 1000);
    aethon::diagnostics::SloSlice slice;
    slice.start_time_ns = 100;
    slice.end_time_ns = 200;
    slice.good_events = 1;
    slice.bad_events = 1;
    slice.total_weight = 2;
    slice.bad_weight = 1;
    auto slo = aethon::diagnostics::evaluate_slo_slice(objective, slice, 200);
    auto slo_findings = aethon::diagnostics::findings_from_slos({slo});

    aethon::diagnostics::IncidentReportBuilder builder;
    auto report = builder.id("INC-7")
        .title("Archive write degradation")
        .severity(aethon::diagnostics::IncidentSeverity::high)
        .status(aethon::diagnostics::IncidentStatus::mitigating)
        .opened_at(100)
        .updated_at(300)
        .summary("archive writes are delayed in east")
        .impact({"archive", "east", "delayed writes", 12, 2, 10})
        .finding(metric_findings.front())
        .finding(alert_findings.front())
        .finding(slo_findings.front())
        .alerts({alert})
        .slo_status(slo)
        .metric_rollups(aethon::diagnostics::rollup_series(aethon::diagnostics::group_metric_series(current.samples())))
        .build();
    auto recommended = aethon::diagnostics::recommend_incident_actions(report);
    for (const auto& action : recommended) {
        report.actions.push_back(action);
    }

    auto summary = aethon::diagnostics::summarize_incident_report(report);
    auto text = aethon::diagnostics::render_incident_report_text(report);
    auto json = aethon::diagnostics::render_incident_report_json(report);

    AETHON_REQUIRE(summary.finding_count == 3);
    AETHON_REQUIRE(summary.firing_alert_count == 1);
    AETHON_REQUIRE(summary.exhausted_slo_count == 1);
    AETHON_REQUIRE(!recommended.empty());
    AETHON_REQUIRE(text.find("incident_report") != std::string::npos);
    AETHON_REQUIRE(json.find("\"INC-7\"") != std::string::npos);
}

AETHON_TEST(report_formatter_projects_compact_incident_sections) {
    aethon::diagnostics::AlertEvaluation alert;
    alert.rule_id = "queue-high";
    alert.title = "Queue depth high";
    alert.severity = aethon::diagnostics::AlertSeverity::critical;
    alert.state = aethon::diagnostics::AlertState::active;
    alert.evaluated_at_ns = 200;
    alert.reason = "threshold condition matched";

    aethon::diagnostics::IncidentReport report;
    report.id = "INC-8";
    report.title = "Queue depth regression";
    report.severity = aethon::diagnostics::IncidentSeverity::high;
    report.status = aethon::diagnostics::IncidentStatus::investigating;
    report.opened_at_ns = 100;
    report.updated_at_ns = 200;
    report.findings = aethon::diagnostics::findings_from_alerts({alert});
    report.alerts = {alert};

    auto projection = aethon::diagnostics::compact_incident_projection();
    auto envelope = aethon::diagnostics::project_incident_report(report, projection, 300);
    aethon::diagnostics::ReportFormatter text_formatter;
    aethon::diagnostics::ReportFormatter json_formatter(aethon::diagnostics::ReportFormat::json);

    auto text = text_formatter.render(envelope);
    auto json = json_formatter.render(envelope);

    AETHON_REQUIRE(envelope.sections.size() == 3);
    AETHON_REQUIRE(text.find("Queue depth regression") != std::string::npos);
    AETHON_REQUIRE(json.find("\"sections\"") != std::string::npos);
}
