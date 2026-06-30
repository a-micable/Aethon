#include "aethon/diagnostics/alert_rules.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

bool tags_match(const MetricTags& candidate, const MetricTags& required) {
    for (const auto& [key, value] : required) {
        auto it = candidate.find(key);
        if (it == candidate.end() || it->second != value) {
            return false;
        }
    }
    return true;
}

std::string format_double(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << value;
    return out.str();
}

std::string format_tags(const MetricTags& tags) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [key, value] : tags) {
        if (!first) {
            out << ",";
        }
        first = false;
        out << key << "=" << value;
    }
    return out.str();
}

std::string tags_json(const MetricTags& tags) {
    JsonObject object;
    for (const auto& [key, value] : tags) {
        object.string(key, value);
    }
    return render_json_object(object);
}

JsonObject evidence_json(const AlertEvidence& evidence) {
    JsonObject object;
    object.string("metric_name", evidence.metric_name);
    object.raw("tags", tags_json(evidence.tags));
    if (evidence.observed_value) {
        object.raw("observed_value", format_double(*evidence.observed_value));
    } else {
        object.raw("observed_value", "null");
    }
    if (evidence.comparison_value) {
        object.raw("comparison_value", format_double(*evidence.comparison_value));
    } else {
        object.raw("comparison_value", "null");
    }
    object.number("observed_time_ns", evidence.observed_time_ns);
    object.string("message", evidence.message);
    return object;
}

std::string evidence_array_json(const std::vector<AlertEvidence>& evidence) {
    std::vector<JsonObject> objects;
    objects.reserve(evidence.size());
    for (const auto& item : evidence) {
        objects.push_back(evidence_json(item));
    }
    return render_json_array(objects);
}

std::string labels_json(const std::vector<std::string>& labels) {
    return render_json_string_array(labels);
}

JsonObject evaluation_json(const AlertEvaluation& evaluation) {
    JsonObject object;
    object.string("rule_id", evaluation.rule_id);
    object.string("title", evaluation.title);
    object.string("severity", alert_severity_name(evaluation.severity));
    object.string("state", alert_state_name(evaluation.state));
    object.number("evaluated_at_ns", evaluation.evaluated_at_ns);
    object.number("active_since_ns", evaluation.active_since_ns);
    object.number("active_duration_ns", evaluation.active_duration_ns);
    object.string("reason", evaluation.reason);
    object.raw("labels", labels_json(evaluation.labels));
    object.raw("evidence", evidence_array_json(evaluation.evidence));
    return object;
}

std::optional<MetricSample> newest_sample(const std::vector<MetricSample>& samples) {
    std::optional<MetricSample> newest;
    for (const auto& sample : samples) {
        if (!newest || sample.time_ns >= newest->time_ns) {
            newest = sample;
        }
    }
    return newest;
}

std::optional<MetricSample> oldest_sample(const std::vector<MetricSample>& samples) {
    std::optional<MetricSample> oldest;
    for (const auto& sample : samples) {
        if (!oldest || sample.time_ns < oldest->time_ns) {
            oldest = sample;
        }
    }
    return oldest;
}

AlertEvaluation base_evaluation(const AlertRule& rule, const AlertEvaluationContext& context) {
    return AlertEvaluation{
        rule.id,
        rule.title,
        rule.severity,
        AlertState::inactive,
        context.evaluation_time_ns,
        0,
        0,
        {},
        rule.labels,
        {},
    };
}

bool rule_suppressed(const AlertRule& rule, std::uint64_t time_ns, std::string& reason) {
    for (const auto& suppression : rule.suppressions) {
        if (suppression.active_at(time_ns)) {
            reason = suppression.reason;
            return true;
        }
    }
    return false;
}

std::vector<MetricSample> select_current_samples(const MetricSnapshot& snapshot,
                                                 std::string_view metric_name,
                                                 const MetricTags& tags) {
    std::vector<MetricSample> selected;
    for (const auto& sample : snapshot.samples()) {
        if (sample.identity.name == metric_name && tags_match(sample.identity.tags, tags)) {
            selected.push_back(sample);
        }
    }
    return selected;
}

AlertEvaluation finish_duration_gate(AlertEvaluation evaluation,
                                     const AlertRule& rule,
                                     const std::vector<MetricSample>& history) {
    if (evaluation.state != AlertState::active || rule.min_duration_ns == 0) {
        return evaluation;
    }
    auto earliest = oldest_sample(history);
    auto latest = newest_sample(history);
    if (!earliest || !latest) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "condition has no history for duration gate";
        return evaluation;
    }
    evaluation.active_since_ns = earliest->time_ns;
    evaluation.active_duration_ns = latest->time_ns >= earliest->time_ns
        ? latest->time_ns - earliest->time_ns
        : 0;
    if (evaluation.active_duration_ns < rule.min_duration_ns) {
        evaluation.state = AlertState::inactive;
        evaluation.reason = "condition active but below minimum duration";
    }
    return evaluation;
}

AlertEvaluation evaluate_threshold(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    const auto& condition = rule.condition.threshold;
    auto current = select_current_samples(context.current, condition.metric_name, condition.required_tags);
    if (current.empty()) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "metric not present in current snapshot";
        return evaluation;
    }

    bool matched = false;
    for (const auto& sample : current) {
        const bool ok = compare_alert_value(sample.value, condition.comparator, condition.threshold);
        AlertEvidence evidence;
        evidence.metric_name = sample.identity.name;
        evidence.tags = sample.identity.tags;
        evidence.observed_value = sample.value;
        evidence.comparison_value = condition.threshold;
        evidence.observed_time_ns = sample.time_ns;
        evidence.message = format_double(sample.value)
            + " "
            + alert_comparator_name(condition.comparator)
            + " "
            + format_double(condition.threshold);
        evaluation.evidence.push_back(std::move(evidence));
        matched = matched || ok;
    }

    evaluation.state = matched ? AlertState::active : AlertState::inactive;
    evaluation.reason = matched ? "threshold condition matched" : "threshold condition did not match";
    auto history = select_alert_metric_history(
        context,
        condition.metric_name,
        condition.required_tags,
        rule.evaluation_window_ns);
    return finish_duration_gate(std::move(evaluation), rule, history);
}

AlertEvaluation evaluate_missing(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    const auto& condition = rule.condition.missing;
    auto current = select_current_samples(context.current, condition.metric_name, condition.required_tags);
    AlertEvidence evidence;
    evidence.metric_name = condition.metric_name;
    evidence.tags = condition.required_tags;
    evidence.observed_time_ns = context.evaluation_time_ns;
    if (current.empty()) {
        evaluation.state = AlertState::active;
        evaluation.reason = "metric is absent";
        evidence.message = "required metric missing";
    } else {
        evaluation.state = AlertState::inactive;
        evaluation.reason = "metric is present";
        evidence.observed_value = newest_sample(current)->value;
        evidence.message = "required metric present";
    }
    evaluation.evidence.push_back(std::move(evidence));
    return evaluation;
}

AlertEvaluation evaluate_stale(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    const auto& condition = rule.condition.stale;
    auto current = select_current_samples(context.current, condition.metric_name, condition.required_tags);
    if (current.empty()) {
        current = select_alert_metric_history(
            context,
            condition.metric_name,
            condition.required_tags,
            rule.evaluation_window_ns == 0 ? condition.max_age_ns : rule.evaluation_window_ns);
    }
    auto newest = newest_sample(current);
    AlertEvidence evidence;
    evidence.metric_name = condition.metric_name;
    evidence.tags = condition.required_tags;
    evidence.comparison_value = static_cast<double>(condition.max_age_ns);
    evidence.observed_time_ns = context.evaluation_time_ns;
    if (!newest) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "metric has no samples";
        evidence.message = "no sample available for staleness check";
        evaluation.evidence.push_back(std::move(evidence));
        return evaluation;
    }
    const auto age = context.evaluation_time_ns >= newest->time_ns
        ? context.evaluation_time_ns - newest->time_ns
        : 0;
    evidence.observed_value = static_cast<double>(age);
    evidence.message = "sample age " + std::to_string(age) + "ns";
    evaluation.evidence.push_back(std::move(evidence));
    evaluation.state = age > condition.max_age_ns ? AlertState::active : AlertState::inactive;
    evaluation.reason = evaluation.state == AlertState::active ? "metric is stale" : "metric is fresh";
    return evaluation;
}

AlertEvaluation evaluate_rate(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    const auto& condition = rule.condition.rate;
    auto history = select_alert_metric_history(
        context,
        condition.metric_name,
        condition.required_tags,
        rule.evaluation_window_ns);
    if (history.size() < 2) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "rate condition needs at least two samples";
        return evaluation;
    }
    auto oldest = oldest_sample(history);
    auto newest = newest_sample(history);
    const auto elapsed_ns = newest->time_ns > oldest->time_ns ? newest->time_ns - oldest->time_ns : 0;
    if (elapsed_ns == 0) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "rate condition has zero elapsed time";
        return evaluation;
    }
    const double elapsed_seconds = static_cast<double>(elapsed_ns) / 1000000000.0;
    const double rate = (newest->value - oldest->value) / elapsed_seconds;
    const bool matched = compare_alert_value(rate, condition.comparator, condition.threshold_per_second);
    AlertEvidence evidence;
    evidence.metric_name = condition.metric_name;
    evidence.tags = condition.required_tags;
    evidence.observed_value = rate;
    evidence.comparison_value = condition.threshold_per_second;
    evidence.observed_time_ns = newest->time_ns;
    evidence.message = "rate " + format_double(rate) + "/s";
    evaluation.evidence.push_back(std::move(evidence));
    evaluation.state = matched ? AlertState::active : AlertState::inactive;
    evaluation.reason = matched ? "rate condition matched" : "rate condition did not match";
    return finish_duration_gate(std::move(evaluation), rule, history);
}

AlertEvaluation evaluate_trend(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    const auto& condition = rule.condition.trend;
    auto history = select_alert_metric_history(
        context,
        condition.metric_name,
        condition.required_tags,
        rule.evaluation_window_ns);
    if (history.size() < 2) {
        evaluation.state = AlertState::no_data;
        evaluation.reason = "trend condition needs at least two samples";
        return evaluation;
    }
    const auto observed = classify_metric_trend(history, condition.noise_floor);
    const bool matched = observed == condition.expected;
    auto newest = newest_sample(history);
    AlertEvidence evidence;
    evidence.metric_name = condition.metric_name;
    evidence.tags = condition.required_tags;
    evidence.observed_time_ns = newest ? newest->time_ns : context.evaluation_time_ns;
    evidence.message = "trend "
        + metric_trend_name(observed)
        + " expected "
        + metric_trend_name(condition.expected);
    evaluation.evidence.push_back(std::move(evidence));
    evaluation.state = matched ? AlertState::active : AlertState::inactive;
    evaluation.reason = matched ? "trend condition matched" : "trend condition did not match";
    return finish_duration_gate(std::move(evaluation), rule, history);
}

} // namespace

bool AlertSuppression::active_at(std::uint64_t time_ns) const noexcept {
    return time_ns >= start_time_ns && time_ns <= end_time_ns;
}

bool AlertEvaluation::firing() const noexcept {
    return state == AlertState::active;
}

void AlertRuleSet::add(AlertRule rule) {
    rules_.push_back(std::move(rule));
}

void AlertRuleSet::clear() {
    rules_.clear();
}

bool AlertRuleSet::empty() const noexcept {
    return rules_.empty();
}

std::size_t AlertRuleSet::size() const noexcept {
    return rules_.size();
}

const std::vector<AlertRule>& AlertRuleSet::rules() const noexcept {
    return rules_;
}

std::optional<AlertRule> AlertRuleSet::find(std::string_view id) const {
    for (const auto& rule : rules_) {
        if (rule.id == id) {
            return rule;
        }
    }
    return std::nullopt;
}

std::vector<AlertEvaluation> AlertRuleSet::evaluate(const AlertEvaluationContext& context) const {
    std::vector<AlertEvaluation> evaluations;
    evaluations.reserve(rules_.size());
    for (const auto& rule : rules_) {
        evaluations.push_back(evaluate_alert_rule(rule, context));
    }
    return evaluations;
}

std::string alert_severity_name(AlertSeverity severity) {
    switch (severity) {
    case AlertSeverity::info:
        return "info";
    case AlertSeverity::warning:
        return "warning";
    case AlertSeverity::critical:
        return "critical";
    }
    return "unknown";
}

std::optional<AlertSeverity> parse_alert_severity(std::string_view value) {
    if (value == "info") {
        return AlertSeverity::info;
    }
    if (value == "warning") {
        return AlertSeverity::warning;
    }
    if (value == "critical") {
        return AlertSeverity::critical;
    }
    return std::nullopt;
}

std::string alert_comparator_name(AlertComparator comparator) {
    switch (comparator) {
    case AlertComparator::less_than:
        return "<";
    case AlertComparator::less_or_equal:
        return "<=";
    case AlertComparator::greater_than:
        return ">";
    case AlertComparator::greater_or_equal:
        return ">=";
    case AlertComparator::equal:
        return "==";
    case AlertComparator::not_equal:
        return "!=";
    }
    return "?";
}

std::optional<AlertComparator> parse_alert_comparator(std::string_view value) {
    if (value == "<") {
        return AlertComparator::less_than;
    }
    if (value == "<=") {
        return AlertComparator::less_or_equal;
    }
    if (value == ">") {
        return AlertComparator::greater_than;
    }
    if (value == ">=") {
        return AlertComparator::greater_or_equal;
    }
    if (value == "==" || value == "=") {
        return AlertComparator::equal;
    }
    if (value == "!=") {
        return AlertComparator::not_equal;
    }
    return std::nullopt;
}

std::string alert_condition_kind_name(AlertConditionKind kind) {
    switch (kind) {
    case AlertConditionKind::threshold:
        return "threshold";
    case AlertConditionKind::missing:
        return "missing";
    case AlertConditionKind::stale:
        return "stale";
    case AlertConditionKind::rate_of_change:
        return "rate_of_change";
    case AlertConditionKind::trend:
        return "trend";
    }
    return "unknown";
}

std::string alert_state_name(AlertState state) {
    switch (state) {
    case AlertState::inactive:
        return "inactive";
    case AlertState::active:
        return "active";
    case AlertState::suppressed:
        return "suppressed";
    case AlertState::no_data:
        return "no_data";
    }
    return "unknown";
}

bool compare_alert_value(double left, AlertComparator comparator, double right) {
    switch (comparator) {
    case AlertComparator::less_than:
        return left < right;
    case AlertComparator::less_or_equal:
        return left <= right;
    case AlertComparator::greater_than:
        return left > right;
    case AlertComparator::greater_or_equal:
        return left >= right;
    case AlertComparator::equal:
        return left == right;
    case AlertComparator::not_equal:
        return left != right;
    }
    return false;
}

AlertCondition threshold_condition(std::string metric_name,
                                   AlertComparator comparator,
                                   double threshold,
                                   MetricTags tags) {
    AlertCondition condition;
    condition.kind = AlertConditionKind::threshold;
    condition.threshold.metric_name = std::move(metric_name);
    condition.threshold.comparator = comparator;
    condition.threshold.threshold = threshold;
    condition.threshold.required_tags = std::move(tags);
    return condition;
}

AlertCondition missing_condition(std::string metric_name, MetricTags tags) {
    AlertCondition condition;
    condition.kind = AlertConditionKind::missing;
    condition.missing.metric_name = std::move(metric_name);
    condition.missing.required_tags = std::move(tags);
    return condition;
}

AlertCondition stale_condition(std::string metric_name,
                               std::uint64_t max_age_ns,
                               MetricTags tags) {
    AlertCondition condition;
    condition.kind = AlertConditionKind::stale;
    condition.stale.metric_name = std::move(metric_name);
    condition.stale.max_age_ns = max_age_ns;
    condition.stale.required_tags = std::move(tags);
    return condition;
}

AlertCondition rate_condition(std::string metric_name,
                              AlertComparator comparator,
                              double threshold_per_second,
                              MetricTags tags) {
    AlertCondition condition;
    condition.kind = AlertConditionKind::rate_of_change;
    condition.rate.metric_name = std::move(metric_name);
    condition.rate.comparator = comparator;
    condition.rate.threshold_per_second = threshold_per_second;
    condition.rate.required_tags = std::move(tags);
    return condition;
}

AlertCondition trend_condition(std::string metric_name,
                               MetricTrend expected,
                               double noise_floor,
                               MetricTags tags) {
    AlertCondition condition;
    condition.kind = AlertConditionKind::trend;
    condition.trend.metric_name = std::move(metric_name);
    condition.trend.expected = expected;
    condition.trend.noise_floor = noise_floor;
    condition.trend.required_tags = std::move(tags);
    return condition;
}

AlertEvaluation evaluate_alert_rule(const AlertRule& rule, const AlertEvaluationContext& context) {
    auto evaluation = base_evaluation(rule, context);
    if (!rule.enabled) {
        evaluation.state = AlertState::inactive;
        evaluation.reason = "rule disabled";
        return evaluation;
    }

    std::string suppression_reason;
    if (rule_suppressed(rule, context.evaluation_time_ns, suppression_reason)) {
        evaluation.state = AlertState::suppressed;
        evaluation.reason = suppression_reason.empty() ? "rule suppressed" : suppression_reason;
        return evaluation;
    }

    switch (rule.condition.kind) {
    case AlertConditionKind::threshold:
        return evaluate_threshold(rule, context);
    case AlertConditionKind::missing:
        return evaluate_missing(rule, context);
    case AlertConditionKind::stale:
        return evaluate_stale(rule, context);
    case AlertConditionKind::rate_of_change:
        return evaluate_rate(rule, context);
    case AlertConditionKind::trend:
        return evaluate_trend(rule, context);
    }
    evaluation.state = AlertState::no_data;
    evaluation.reason = "unknown condition";
    return evaluation;
}

AlertEvaluationSummary summarize_alert_evaluations(const std::vector<AlertEvaluation>& evaluations,
                                                   std::uint64_t evaluated_at_ns) {
    AlertEvaluationSummary summary;
    summary.evaluated_at_ns = evaluated_at_ns;
    summary.total_rules = evaluations.size();
    for (const auto& evaluation : evaluations) {
        switch (evaluation.state) {
        case AlertState::inactive:
            ++summary.inactive_rules;
            break;
        case AlertState::active:
            ++summary.active_rules;
            if (evaluation.severity == AlertSeverity::critical) {
                ++summary.critical_active;
            } else if (evaluation.severity == AlertSeverity::warning) {
                ++summary.warning_active;
            } else {
                ++summary.info_active;
            }
            break;
        case AlertState::suppressed:
            ++summary.suppressed_rules;
            break;
        case AlertState::no_data:
            ++summary.no_data_rules;
            break;
        }
        if (evaluation.reason == "rule disabled") {
            ++summary.disabled_rules;
        }
    }
    return summary;
}

std::vector<AlertEvaluation> firing_alerts(const std::vector<AlertEvaluation>& evaluations) {
    std::vector<AlertEvaluation> selected;
    for (const auto& evaluation : evaluations) {
        if (evaluation.firing()) {
            selected.push_back(evaluation);
        }
    }
    return selected;
}

std::vector<AlertEvaluation> alerts_by_severity(const std::vector<AlertEvaluation>& evaluations,
                                                AlertSeverity severity) {
    std::vector<AlertEvaluation> selected;
    for (const auto& evaluation : evaluations) {
        if (evaluation.severity == severity) {
            selected.push_back(evaluation);
        }
    }
    return selected;
}

std::vector<MetricSample> select_alert_metric_history(const AlertEvaluationContext& context,
                                                      std::string_view metric_name,
                                                      const MetricTags& tags,
                                                      std::uint64_t window_ns) {
    const auto start_ns = window_ns == 0 || context.evaluation_time_ns < window_ns
        ? 0
        : context.evaluation_time_ns - window_ns;
    std::vector<MetricSample> selected;
    for (const auto& sample : context.history) {
        if (sample.identity.name != metric_name) {
            continue;
        }
        if (!tags_match(sample.identity.tags, tags)) {
            continue;
        }
        if (sample.time_ns >= start_ns && sample.time_ns <= context.evaluation_time_ns) {
            selected.push_back(sample);
        }
    }
    for (const auto& sample : context.current.samples()) {
        if (sample.identity.name != metric_name) {
            continue;
        }
        if (!tags_match(sample.identity.tags, tags)) {
            continue;
        }
        if (sample.time_ns >= start_ns && sample.time_ns <= context.evaluation_time_ns) {
            selected.push_back(sample);
        }
    }
    std::sort(
        selected.begin(),
        selected.end(),
        [](const MetricSample& left, const MetricSample& right) {
            return left.time_ns < right.time_ns;
        });
    return selected;
}

std::string render_alert_evaluations_text(const std::vector<AlertEvaluation>& evaluations) {
    TextTable table;
    table.headers = {"rule", "severity", "state", "reason", "evidence"};
    for (const auto& evaluation : evaluations) {
        std::ostringstream evidence;
        for (std::size_t i = 0; i < evaluation.evidence.size(); ++i) {
            if (i != 0) {
                evidence << "; ";
            }
            const auto& item = evaluation.evidence[i];
            evidence << item.metric_name;
            if (!item.tags.empty()) {
                evidence << "[" << format_tags(item.tags) << "]";
            }
            if (item.observed_value) {
                evidence << "=" << format_double(*item.observed_value);
            }
        }
        add_row(
            table,
            {
                evaluation.rule_id,
                alert_severity_name(evaluation.severity),
                alert_state_name(evaluation.state),
                evaluation.reason,
                evidence.str(),
            });
    }
    return render_text_table(table);
}

std::string render_alert_evaluations_json(const std::vector<AlertEvaluation>& evaluations) {
    std::vector<JsonObject> objects;
    objects.reserve(evaluations.size());
    for (const auto& evaluation : evaluations) {
        objects.push_back(evaluation_json(evaluation));
    }
    return render_json_array(objects);
}

std::string render_alert_summary_text(const AlertEvaluationSummary& summary) {
    TextTable table;
    table.headers = {"evaluated_at_ns", "total", "active", "suppressed", "no_data", "critical", "warning", "info"};
    add_row(
        table,
        {
            std::to_string(summary.evaluated_at_ns),
            std::to_string(summary.total_rules),
            std::to_string(summary.active_rules),
            std::to_string(summary.suppressed_rules),
            std::to_string(summary.no_data_rules),
            std::to_string(summary.critical_active),
            std::to_string(summary.warning_active),
            std::to_string(summary.info_active),
        });
    return render_text_table(table);
}

} // namespace aethon::diagnostics
