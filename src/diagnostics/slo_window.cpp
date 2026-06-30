#include "aethon/diagnostics/slo_window.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

std::string format_double(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << value;
    return out.str();
}

double allowed_error_ratio(double target_ratio) {
    if (target_ratio >= 1.0) {
        return 0.0;
    }
    if (target_ratio <= 0.0) {
        return 1.0;
    }
    return 1.0 - target_ratio;
}

SloStatus classify_budget(double budget_used_ratio) {
    if (budget_used_ratio < 0.0) {
        return SloStatus::unknown;
    }
    if (budget_used_ratio >= 1.0) {
        return SloStatus::exhausted;
    }
    if (budget_used_ratio >= 0.75) {
        return SloStatus::warning;
    }
    return SloStatus::healthy;
}

void add_event_to_slice(SloSlice& slice, const SloEvent& event) {
    switch (event.kind) {
    case SloEventKind::good:
        ++slice.good_events;
        slice.total_weight += event.weight;
        break;
    case SloEventKind::bad:
        ++slice.bad_events;
        slice.total_weight += event.weight;
        slice.bad_weight += event.weight;
        break;
    case SloEventKind::ignored:
        ++slice.ignored_events;
        break;
    }
}

std::uint64_t window_start_for(const SloObjective& objective, std::uint64_t evaluated_at_ns) {
    if (objective.window_ns == 0 || evaluated_at_ns < objective.window_ns) {
        return 0;
    }
    return evaluated_at_ns - objective.window_ns;
}

JsonObject budget_json_object(const SloBudgetStatus& status) {
    JsonObject object;
    object.string("id", status.objective.id);
    object.string("name", status.objective.name);
    object.raw("target_ratio", format_double(status.objective.target_ratio));
    object.number("evaluated_at_ns", status.evaluated_at_ns);
    object.number("window_start_ns", status.window_start_ns);
    object.number("window_end_ns", status.window_end_ns);
    object.number("good_events", status.good_events);
    object.number("bad_events", status.bad_events);
    object.number("ignored_events", status.ignored_events);
    object.number("total_weight", status.total_weight);
    object.number("bad_weight", status.bad_weight);
    object.raw("success_ratio", format_double(status.success_ratio));
    object.raw("error_ratio", format_double(status.error_ratio));
    object.raw("allowed_error_ratio", format_double(status.allowed_error_ratio));
    object.raw("budget_used_ratio", format_double(status.budget_used_ratio));
    object.raw("budget_remaining_ratio", format_double(status.budget_remaining_ratio));
    object.raw("burn_rate", format_double(status.burn_rate));
    object.string("status", slo_status_name(status.status));
    return object;
}

JsonObject burn_window_json_object(const SloBurnRateWindow& window) {
    JsonObject object;
    object.string("label", window.label);
    object.number("window_ns", window.window_ns);
    object.raw("burn_rate", format_double(window.burn_rate));
    object.raw("budget_used_ratio", format_double(window.budget_used_ratio));
    object.string("status", slo_status_name(window.status));
    return object;
}

std::string burn_windows_json(const std::vector<SloBurnRateWindow>& windows) {
    std::vector<JsonObject> objects;
    objects.reserve(windows.size());
    for (const auto& window : windows) {
        objects.push_back(burn_window_json_object(window));
    }
    return render_json_array(objects);
}

std::string duration_label(std::uint64_t ns) {
    if (ns == 0) {
        return "all";
    }
    const auto seconds = ns / 1000000000ULL;
    if (seconds >= 86400 && seconds % 86400 == 0) {
        return std::to_string(seconds / 86400) + "d";
    }
    if (seconds >= 3600 && seconds % 3600 == 0) {
        return std::to_string(seconds / 3600) + "h";
    }
    if (seconds >= 60 && seconds % 60 == 0) {
        return std::to_string(seconds / 60) + "m";
    }
    return std::to_string(seconds) + "s";
}

std::string recommendation_for(const MultiWindowSloReport& report) {
    if (report.primary.status == SloStatus::exhausted) {
        return "error budget exhausted; stop risky changes and prioritize mitigation";
    }
    for (const auto& window : report.windows) {
        if (window.status == SloStatus::exhausted && window.burn_rate >= 2.0) {
            return "fast burn detected; page the owning team and inspect recent deploys";
        }
    }
    if (report.primary.status == SloStatus::warning) {
        return "budget is under pressure; reduce risk and monitor the short windows";
    }
    if (report.primary.status == SloStatus::healthy) {
        return "budget is healthy";
    }
    return "insufficient SLO data";
}

} // namespace

SloWindow::SloWindow(SloObjective objective)
    : objective_(std::move(objective)) {
}

void SloWindow::add(SloEvent event) {
    events_.push_back(std::move(event));
}

void SloWindow::good(std::uint64_t time_ns, std::uint64_t weight, std::string source) {
    add(SloEvent{
        time_ns,
        SloEventKind::good,
        weight,
        std::move(source),
        {},
    });
}

void SloWindow::bad(std::uint64_t time_ns,
                    std::uint64_t weight,
                    std::string source,
                    std::string note) {
    add(SloEvent{
        time_ns,
        SloEventKind::bad,
        weight,
        std::move(source),
        std::move(note),
    });
}

void SloWindow::ignored(std::uint64_t time_ns, std::string source, std::string note) {
    add(SloEvent{
        time_ns,
        SloEventKind::ignored,
        0,
        std::move(source),
        std::move(note),
    });
}

void SloWindow::clear() {
    events_.clear();
}

bool SloWindow::empty() const noexcept {
    return events_.empty();
}

std::size_t SloWindow::size() const noexcept {
    return events_.size();
}

const SloObjective& SloWindow::objective() const noexcept {
    return objective_;
}

const std::vector<SloEvent>& SloWindow::events() const noexcept {
    return events_;
}

std::vector<SloEvent> SloWindow::events_between(std::uint64_t start_ns,
                                                std::uint64_t end_ns) const {
    std::vector<SloEvent> selected;
    for (const auto& event : events_) {
        if (event.time_ns >= start_ns && event.time_ns <= end_ns) {
            selected.push_back(event);
        }
    }
    std::sort(
        selected.begin(),
        selected.end(),
        [](const SloEvent& left, const SloEvent& right) {
            return left.time_ns < right.time_ns;
        });
    return selected;
}

SloSlice SloWindow::slice(std::uint64_t start_ns, std::uint64_t end_ns) const {
    SloSlice slice;
    slice.start_time_ns = start_ns;
    slice.end_time_ns = end_ns;
    for (const auto& event : events_) {
        if (event.time_ns >= start_ns && event.time_ns <= end_ns) {
            add_event_to_slice(slice, event);
        }
    }
    return slice;
}

std::vector<SloSlice> SloWindow::partition(std::uint64_t start_ns,
                                           std::uint64_t end_ns,
                                           std::uint64_t slice_ns) const {
    std::vector<SloSlice> slices;
    if (slice_ns == 0 || end_ns < start_ns) {
        return slices;
    }
    for (auto cursor = start_ns; cursor <= end_ns;) {
        const auto next = cursor + slice_ns > end_ns ? end_ns : cursor + slice_ns;
        slices.push_back(slice(cursor, next));
        if (next == end_ns) {
            break;
        }
        cursor = next + 1;
    }
    return slices;
}

SloBudgetStatus SloWindow::evaluate(std::uint64_t evaluated_at_ns) const {
    const auto start = window_start_for(objective_, evaluated_at_ns);
    return evaluate_slo_slice(objective_, slice(start, evaluated_at_ns), evaluated_at_ns);
}

std::string slo_event_kind_name(SloEventKind kind) {
    switch (kind) {
    case SloEventKind::good:
        return "good";
    case SloEventKind::bad:
        return "bad";
    case SloEventKind::ignored:
        return "ignored";
    }
    return "unknown";
}

std::optional<SloEventKind> parse_slo_event_kind(std::string_view value) {
    if (value == "good") {
        return SloEventKind::good;
    }
    if (value == "bad") {
        return SloEventKind::bad;
    }
    if (value == "ignored") {
        return SloEventKind::ignored;
    }
    return std::nullopt;
}

std::string slo_status_name(SloStatus status) {
    switch (status) {
    case SloStatus::unknown:
        return "unknown";
    case SloStatus::healthy:
        return "healthy";
    case SloStatus::warning:
        return "warning";
    case SloStatus::exhausted:
        return "exhausted";
    }
    return "unknown";
}

SloObjective make_availability_slo(std::string id,
                                   std::string name,
                                   double target_ratio,
                                   std::uint64_t window_ns) {
    SloObjective objective;
    objective.id = std::move(id);
    objective.name = std::move(name);
    objective.target_ratio = target_ratio;
    objective.window_ns = window_ns;
    objective.description = "availability";
    return objective;
}

SloObjective make_latency_slo(std::string id,
                              std::string name,
                              double target_ratio,
                              std::uint64_t window_ns,
                              std::uint64_t threshold_ns) {
    SloObjective objective;
    objective.id = std::move(id);
    objective.name = std::move(name);
    objective.target_ratio = target_ratio;
    objective.window_ns = window_ns;
    objective.description = "latency <= " + std::to_string(threshold_ns) + "ns";
    return objective;
}

SloBudgetStatus evaluate_slo_slice(const SloObjective& objective,
                                   const SloSlice& slice,
                                   std::uint64_t evaluated_at_ns) {
    SloBudgetStatus status;
    status.objective = objective;
    status.evaluated_at_ns = evaluated_at_ns;
    status.window_start_ns = slice.start_time_ns;
    status.window_end_ns = slice.end_time_ns;
    status.good_events = slice.good_events;
    status.bad_events = slice.bad_events;
    status.ignored_events = slice.ignored_events;
    status.total_weight = slice.total_weight;
    status.bad_weight = slice.bad_weight;
    status.allowed_error_ratio = allowed_error_ratio(objective.target_ratio);
    if (slice.total_weight == 0) {
        status.status = SloStatus::unknown;
        return status;
    }
    status.error_ratio = static_cast<double>(slice.bad_weight) / static_cast<double>(slice.total_weight);
    status.success_ratio = 1.0 - status.error_ratio;
    if (status.allowed_error_ratio == 0.0) {
        status.budget_used_ratio = status.error_ratio == 0.0 ? 0.0 : 1.0;
    } else {
        status.budget_used_ratio = status.error_ratio / status.allowed_error_ratio;
    }
    status.budget_remaining_ratio = std::max(0.0, 1.0 - status.budget_used_ratio);
    status.burn_rate = status.budget_used_ratio;
    status.status = classify_budget(status.budget_used_ratio);
    return status;
}

MultiWindowSloReport evaluate_multi_window_slo(const SloWindow& window,
                                               std::uint64_t evaluated_at_ns,
                                               const std::vector<std::uint64_t>& windows_ns) {
    MultiWindowSloReport report;
    report.objective = window.objective();
    report.evaluated_at_ns = evaluated_at_ns;
    report.primary = window.evaluate(evaluated_at_ns);
    for (auto duration_ns : windows_ns) {
        SloObjective scoped = window.objective();
        scoped.window_ns = duration_ns;
        const auto start = duration_ns == 0 || evaluated_at_ns < duration_ns
            ? 0
            : evaluated_at_ns - duration_ns;
        auto status = evaluate_slo_slice(scoped, window.slice(start, evaluated_at_ns), evaluated_at_ns);
        report.windows.push_back(SloBurnRateWindow{
            duration_label(duration_ns),
            duration_ns,
            status.burn_rate,
            status.budget_used_ratio,
            status.status,
        });
    }
    report.recommendation = recommendation_for(report);
    return report;
}

SloWindow slo_from_latency_histogram(const SloObjective& objective,
                                     const LatencyHistogram& histogram,
                                     std::uint64_t threshold_ns) {
    SloWindow window(objective);
    for (const auto& observation : histogram.observations()) {
        if (observation.value_ns <= threshold_ns) {
            window.good(observation.time_ns, 1, "latency");
        } else {
            window.bad(observation.time_ns, 1, "latency", "above latency threshold");
        }
    }
    return window;
}

SloWindow slo_from_metric_samples(const SloObjective& objective,
                                  const std::vector<MetricSample>& samples,
                                  double bad_threshold,
                                  bool greater_is_bad) {
    SloWindow window(objective);
    for (const auto& sample : samples) {
        const bool bad = greater_is_bad
            ? sample.value > bad_threshold
            : sample.value < bad_threshold;
        if (bad) {
            window.bad(sample.time_ns, 1, sample.identity.name, "metric threshold failed");
        } else {
            window.good(sample.time_ns, 1, sample.identity.name);
        }
    }
    return window;
}

std::vector<SloEvent> merge_slo_events(const std::vector<SloEvent>& left,
                                       const std::vector<SloEvent>& right) {
    std::vector<SloEvent> merged;
    merged.reserve(left.size() + right.size());
    merged.insert(merged.end(), left.begin(), left.end());
    merged.insert(merged.end(), right.begin(), right.end());
    std::sort(
        merged.begin(),
        merged.end(),
        [](const SloEvent& lhs, const SloEvent& rhs) {
            if (lhs.time_ns != rhs.time_ns) {
                return lhs.time_ns < rhs.time_ns;
            }
            return slo_event_kind_name(lhs.kind) < slo_event_kind_name(rhs.kind);
        });
    return merged;
}

std::string render_slo_budget_text(const SloBudgetStatus& status) {
    TextTable table;
    table.headers = {"slo", "status", "success", "error", "budget_used", "remaining", "good", "bad", "ignored"};
    add_row(
        table,
        {
            status.objective.id,
            slo_status_name(status.status),
            format_double(status.success_ratio),
            format_double(status.error_ratio),
            format_double(status.budget_used_ratio),
            format_double(status.budget_remaining_ratio),
            std::to_string(status.good_events),
            std::to_string(status.bad_events),
            std::to_string(status.ignored_events),
        });
    return render_text_table(table);
}

std::string render_slo_budget_json(const SloBudgetStatus& status) {
    return render_json_object(budget_json_object(status));
}

std::string render_multi_window_slo_text(const MultiWindowSloReport& report) {
    TextTable table;
    table.headers = {"window", "status", "burn_rate", "budget_used"};
    for (const auto& window : report.windows) {
        add_row(
            table,
            {
                window.label,
                slo_status_name(window.status),
                format_double(window.burn_rate),
                format_double(window.budget_used_ratio),
            });
    }
    std::ostringstream out;
    out << "slo_primary\n"
        << render_slo_budget_text(report.primary)
        << "slo_windows\n"
        << render_text_table(table)
        << "recommendation: "
        << report.recommendation
        << "\n";
    return out.str();
}

std::string render_multi_window_slo_json(const MultiWindowSloReport& report) {
    JsonObject root;
    root.string("id", report.objective.id);
    root.string("name", report.objective.name);
    root.number("evaluated_at_ns", report.evaluated_at_ns);
    root.raw("primary", render_json_object(budget_json_object(report.primary)));
    root.raw("windows", burn_windows_json(report.windows));
    root.string("recommendation", report.recommendation);
    return render_json_object(root);
}

} // namespace aethon::diagnostics
