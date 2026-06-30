#pragma once

#include "aethon/diagnostics/metrics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

enum class AlertSeverity {
    info,
    warning,
    critical,
};

enum class AlertComparator {
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal,
    equal,
    not_equal,
};

enum class AlertConditionKind {
    threshold,
    missing,
    stale,
    rate_of_change,
    trend,
};

enum class AlertState {
    inactive,
    active,
    suppressed,
    no_data,
};

struct AlertThresholdCondition {
    std::string metric_name;
    MetricTags required_tags;
    AlertComparator comparator = AlertComparator::greater_or_equal;
    double threshold = 0.0;
};

struct AlertMissingCondition {
    std::string metric_name;
    MetricTags required_tags;
};

struct AlertStaleCondition {
    std::string metric_name;
    MetricTags required_tags;
    std::uint64_t max_age_ns = 0;
};

struct AlertRateCondition {
    std::string metric_name;
    MetricTags required_tags;
    AlertComparator comparator = AlertComparator::greater_or_equal;
    double threshold_per_second = 0.0;
};

struct AlertTrendCondition {
    std::string metric_name;
    MetricTags required_tags;
    MetricTrend expected = MetricTrend::rising;
    double noise_floor = 0.0;
};

struct AlertCondition {
    AlertConditionKind kind = AlertConditionKind::threshold;
    AlertThresholdCondition threshold;
    AlertMissingCondition missing;
    AlertStaleCondition stale;
    AlertRateCondition rate;
    AlertTrendCondition trend;
};

struct AlertSuppression {
    std::string reason;
    std::uint64_t start_time_ns = 0;
    std::uint64_t end_time_ns = 0;

    [[nodiscard]] bool active_at(std::uint64_t time_ns) const noexcept;
};

struct AlertRule {
    std::string id;
    std::string title;
    std::string description;
    AlertSeverity severity = AlertSeverity::warning;
    AlertCondition condition;
    std::uint64_t evaluation_window_ns = 0;
    std::uint64_t min_duration_ns = 0;
    std::vector<std::string> labels;
    std::vector<AlertSuppression> suppressions;
    bool enabled = true;
};

struct AlertEvidence {
    std::string metric_name;
    MetricTags tags;
    std::optional<double> observed_value;
    std::optional<double> comparison_value;
    std::uint64_t observed_time_ns = 0;
    std::string message;
};

struct AlertEvaluation {
    std::string rule_id;
    std::string title;
    AlertSeverity severity = AlertSeverity::warning;
    AlertState state = AlertState::inactive;
    std::uint64_t evaluated_at_ns = 0;
    std::uint64_t active_since_ns = 0;
    std::uint64_t active_duration_ns = 0;
    std::string reason;
    std::vector<std::string> labels;
    std::vector<AlertEvidence> evidence;

    [[nodiscard]] bool firing() const noexcept;
};

struct AlertEvaluationSummary {
    std::uint64_t evaluated_at_ns = 0;
    std::size_t total_rules = 0;
    std::size_t disabled_rules = 0;
    std::size_t inactive_rules = 0;
    std::size_t active_rules = 0;
    std::size_t suppressed_rules = 0;
    std::size_t no_data_rules = 0;
    std::size_t critical_active = 0;
    std::size_t warning_active = 0;
    std::size_t info_active = 0;
};

struct AlertEvaluationContext {
    MetricSnapshot current;
    std::vector<MetricSample> history;
    std::uint64_t evaluation_time_ns = 0;
};

class AlertRuleSet {
public:
    void add(AlertRule rule);
    void clear();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const std::vector<AlertRule>& rules() const noexcept;
    [[nodiscard]] std::optional<AlertRule> find(std::string_view id) const;
    [[nodiscard]] std::vector<AlertEvaluation> evaluate(const AlertEvaluationContext& context) const;

private:
    std::vector<AlertRule> rules_;
};

[[nodiscard]] std::string alert_severity_name(AlertSeverity severity);
[[nodiscard]] std::optional<AlertSeverity> parse_alert_severity(std::string_view value);
[[nodiscard]] std::string alert_comparator_name(AlertComparator comparator);
[[nodiscard]] std::optional<AlertComparator> parse_alert_comparator(std::string_view value);
[[nodiscard]] std::string alert_condition_kind_name(AlertConditionKind kind);
[[nodiscard]] std::string alert_state_name(AlertState state);
[[nodiscard]] bool compare_alert_value(double left, AlertComparator comparator, double right);
[[nodiscard]] AlertCondition threshold_condition(std::string metric_name,
                                                AlertComparator comparator,
                                                double threshold,
                                                MetricTags tags = {});
[[nodiscard]] AlertCondition missing_condition(std::string metric_name, MetricTags tags = {});
[[nodiscard]] AlertCondition stale_condition(std::string metric_name,
                                            std::uint64_t max_age_ns,
                                            MetricTags tags = {});
[[nodiscard]] AlertCondition rate_condition(std::string metric_name,
                                           AlertComparator comparator,
                                           double threshold_per_second,
                                           MetricTags tags = {});
[[nodiscard]] AlertCondition trend_condition(std::string metric_name,
                                            MetricTrend expected,
                                            double noise_floor = 0.0,
                                            MetricTags tags = {});
[[nodiscard]] AlertEvaluation evaluate_alert_rule(const AlertRule& rule,
                                                  const AlertEvaluationContext& context);
[[nodiscard]] AlertEvaluationSummary summarize_alert_evaluations(const std::vector<AlertEvaluation>& evaluations,
                                                                 std::uint64_t evaluated_at_ns);
[[nodiscard]] std::vector<AlertEvaluation> firing_alerts(const std::vector<AlertEvaluation>& evaluations);
[[nodiscard]] std::vector<AlertEvaluation> alerts_by_severity(const std::vector<AlertEvaluation>& evaluations,
                                                              AlertSeverity severity);
[[nodiscard]] std::vector<MetricSample> select_alert_metric_history(const AlertEvaluationContext& context,
                                                                    std::string_view metric_name,
                                                                    const MetricTags& tags,
                                                                    std::uint64_t window_ns);
[[nodiscard]] std::string render_alert_evaluations_text(const std::vector<AlertEvaluation>& evaluations);
[[nodiscard]] std::string render_alert_evaluations_json(const std::vector<AlertEvaluation>& evaluations);
[[nodiscard]] std::string render_alert_summary_text(const AlertEvaluationSummary& summary);

} // namespace aethon::diagnostics
