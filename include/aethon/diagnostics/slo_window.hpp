#pragma once

#include "aethon/diagnostics/metrics.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

enum class SloEventKind {
    good,
    bad,
    ignored,
};

enum class SloStatus {
    unknown,
    healthy,
    warning,
    exhausted,
};

struct SloEvent {
    std::uint64_t time_ns = 0;
    SloEventKind kind = SloEventKind::good;
    std::uint64_t weight = 1;
    std::string source;
    std::string note;
};

struct SloObjective {
    std::string id;
    std::string name;
    double target_ratio = 0.999;
    std::uint64_t window_ns = 0;
    std::string description;
};

struct SloSlice {
    std::uint64_t start_time_ns = 0;
    std::uint64_t end_time_ns = 0;
    std::uint64_t good_events = 0;
    std::uint64_t bad_events = 0;
    std::uint64_t ignored_events = 0;
    std::uint64_t total_weight = 0;
    std::uint64_t bad_weight = 0;
};

struct SloBudgetStatus {
    SloObjective objective;
    std::uint64_t evaluated_at_ns = 0;
    std::uint64_t window_start_ns = 0;
    std::uint64_t window_end_ns = 0;
    std::uint64_t good_events = 0;
    std::uint64_t bad_events = 0;
    std::uint64_t ignored_events = 0;
    std::uint64_t total_weight = 0;
    std::uint64_t bad_weight = 0;
    double success_ratio = 0.0;
    double error_ratio = 0.0;
    double allowed_error_ratio = 0.0;
    double budget_used_ratio = 0.0;
    double budget_remaining_ratio = 0.0;
    double burn_rate = 0.0;
    SloStatus status = SloStatus::unknown;
};

struct SloBurnRateWindow {
    std::string label;
    std::uint64_t window_ns = 0;
    double burn_rate = 0.0;
    double budget_used_ratio = 0.0;
    SloStatus status = SloStatus::unknown;
};

struct MultiWindowSloReport {
    SloObjective objective;
    std::uint64_t evaluated_at_ns = 0;
    SloBudgetStatus primary;
    std::vector<SloBurnRateWindow> windows;
    std::string recommendation;
};

class SloWindow {
public:
    explicit SloWindow(SloObjective objective = {});

    void add(SloEvent event);
    void good(std::uint64_t time_ns, std::uint64_t weight = 1, std::string source = {});
    void bad(std::uint64_t time_ns, std::uint64_t weight = 1, std::string source = {}, std::string note = {});
    void ignored(std::uint64_t time_ns, std::string source = {}, std::string note = {});
    void clear();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] const SloObjective& objective() const noexcept;
    [[nodiscard]] const std::vector<SloEvent>& events() const noexcept;
    [[nodiscard]] std::vector<SloEvent> events_between(std::uint64_t start_ns,
                                                       std::uint64_t end_ns) const;
    [[nodiscard]] SloSlice slice(std::uint64_t start_ns, std::uint64_t end_ns) const;
    [[nodiscard]] std::vector<SloSlice> partition(std::uint64_t start_ns,
                                                  std::uint64_t end_ns,
                                                  std::uint64_t slice_ns) const;
    [[nodiscard]] SloBudgetStatus evaluate(std::uint64_t evaluated_at_ns) const;

private:
    SloObjective objective_;
    std::vector<SloEvent> events_;
};

[[nodiscard]] std::string slo_event_kind_name(SloEventKind kind);
[[nodiscard]] std::optional<SloEventKind> parse_slo_event_kind(std::string_view value);
[[nodiscard]] std::string slo_status_name(SloStatus status);
[[nodiscard]] SloObjective make_availability_slo(std::string id,
                                                std::string name,
                                                double target_ratio,
                                                std::uint64_t window_ns);
[[nodiscard]] SloObjective make_latency_slo(std::string id,
                                           std::string name,
                                           double target_ratio,
                                           std::uint64_t window_ns,
                                           std::uint64_t threshold_ns);
[[nodiscard]] SloBudgetStatus evaluate_slo_slice(const SloObjective& objective,
                                                 const SloSlice& slice,
                                                 std::uint64_t evaluated_at_ns);
[[nodiscard]] MultiWindowSloReport evaluate_multi_window_slo(const SloWindow& window,
                                                            std::uint64_t evaluated_at_ns,
                                                            const std::vector<std::uint64_t>& windows_ns);
[[nodiscard]] SloWindow slo_from_latency_histogram(const SloObjective& objective,
                                                  const LatencyHistogram& histogram,
                                                  std::uint64_t threshold_ns);
[[nodiscard]] SloWindow slo_from_metric_samples(const SloObjective& objective,
                                                const std::vector<MetricSample>& samples,
                                                double bad_threshold,
                                                bool greater_is_bad);
[[nodiscard]] std::vector<SloEvent> merge_slo_events(const std::vector<SloEvent>& left,
                                                     const std::vector<SloEvent>& right);
[[nodiscard]] std::string render_slo_budget_text(const SloBudgetStatus& status);
[[nodiscard]] std::string render_slo_budget_json(const SloBudgetStatus& status);
[[nodiscard]] std::string render_multi_window_slo_text(const MultiWindowSloReport& report);
[[nodiscard]] std::string render_multi_window_slo_json(const MultiWindowSloReport& report);

} // namespace aethon::diagnostics
