#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

using MetricTags = std::map<std::string, std::string>;

enum class MetricKind {
    counter,
    gauge,
    latency,
    ratio,
    bytes,
};

enum class MetricTrend {
    missing,
    flat,
    rising,
    falling,
    volatile_value,
};

struct MetricIdentity {
    std::string name;
    MetricTags tags;

    [[nodiscard]] bool empty() const noexcept;
};

struct MetricSample {
    MetricIdentity identity;
    MetricKind kind = MetricKind::gauge;
    double value = 0.0;
    std::uint64_t time_ns = 0;
    std::string unit;
    std::string description;
};

struct MetricSeries {
    MetricIdentity identity;
    MetricKind kind = MetricKind::gauge;
    std::string unit;
    std::vector<MetricSample> samples;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
};

struct MetricRollup {
    MetricIdentity identity;
    MetricKind kind = MetricKind::gauge;
    std::string unit;
    std::uint64_t first_time_ns = 0;
    std::uint64_t last_time_ns = 0;
    std::size_t sample_count = 0;
    double min = 0.0;
    double max = 0.0;
    double sum = 0.0;
    double average = 0.0;
    double latest = 0.0;
    double delta = 0.0;
};

struct MetricDelta {
    MetricIdentity identity;
    MetricKind kind = MetricKind::gauge;
    std::string unit;
    std::optional<double> previous;
    std::optional<double> current;
    double absolute_change = 0.0;
    double relative_change = 0.0;
    MetricTrend trend = MetricTrend::missing;
};

struct MetricSnapshotSummary {
    std::uint64_t capture_time_ns = 0;
    std::size_t metric_count = 0;
    std::size_t counter_count = 0;
    std::size_t gauge_count = 0;
    std::size_t latency_count = 0;
    std::size_t ratio_count = 0;
    std::size_t bytes_count = 0;
    double total_counter_value = 0.0;
    double total_byte_value = 0.0;
};

class MetricSnapshot {
public:
    void add(MetricSample sample);
    void clear();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint64_t capture_time_ns() const noexcept;
    [[nodiscard]] const std::vector<MetricSample>& samples() const noexcept;
    [[nodiscard]] std::vector<MetricSample> find_by_name(std::string_view name) const;
    [[nodiscard]] std::vector<MetricSample> find_by_tag(std::string_view key,
                                                        std::string_view value) const;
    [[nodiscard]] std::optional<MetricSample> latest(std::string_view name) const;
    [[nodiscard]] std::optional<MetricSample> latest(const MetricIdentity& identity) const;
    [[nodiscard]] MetricSnapshotSummary summary() const;

private:
    std::vector<MetricSample> samples_;
    std::uint64_t capture_time_ns_ = 0;
};

class MetricSnapshotBuilder {
public:
    MetricSnapshotBuilder& capture_time(std::uint64_t time_ns);
    MetricSnapshotBuilder& counter(std::string name,
                                   double value,
                                   MetricTags tags = {},
                                   std::string unit = "count");
    MetricSnapshotBuilder& gauge(std::string name,
                                 double value,
                                 MetricTags tags = {},
                                 std::string unit = "");
    MetricSnapshotBuilder& latency(std::string name,
                                   double value_ns,
                                   MetricTags tags = {});
    MetricSnapshotBuilder& ratio(std::string name,
                                 double value,
                                 MetricTags tags = {});
    MetricSnapshotBuilder& bytes(std::string name,
                                double value,
                                MetricTags tags = {});
    MetricSnapshot build() const;
    void reset();

private:
    MetricSnapshot snapshot_;
    std::uint64_t capture_time_ns_ = 0;
};

struct LatencyBucket {
    std::uint64_t upper_bound_ns = 0;
    std::uint64_t count = 0;
};

struct LatencyObservation {
    std::uint64_t value_ns = 0;
    std::uint64_t time_ns = 0;
    MetricTags tags;
};

struct LatencyHistogramStats {
    std::uint64_t count = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t sum_ns = 0;
    double mean_ns = 0.0;
    double p50_ns = 0.0;
    double p90_ns = 0.0;
    double p95_ns = 0.0;
    double p99_ns = 0.0;
    std::uint64_t overflow_count = 0;
};

class LatencyHistogram {
public:
    LatencyHistogram();
    explicit LatencyHistogram(std::vector<std::uint64_t> bucket_bounds_ns);

    void observe(std::uint64_t value_ns, std::uint64_t time_ns = 0, MetricTags tags = {});
    void merge(const LatencyHistogram& other);
    void clear();

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::uint64_t count() const noexcept;
    [[nodiscard]] std::uint64_t overflow_count() const noexcept;
    [[nodiscard]] const std::vector<LatencyBucket>& buckets() const noexcept;
    [[nodiscard]] const std::vector<LatencyObservation>& observations() const noexcept;
    [[nodiscard]] LatencyHistogramStats stats() const;
    [[nodiscard]] std::vector<LatencyObservation> observations_between(std::uint64_t start_ns,
                                                                       std::uint64_t end_ns) const;
    [[nodiscard]] std::vector<LatencyObservation> observations_with_tag(std::string_view key,
                                                                       std::string_view value) const;

private:
    std::vector<LatencyBucket> buckets_;
    std::vector<LatencyObservation> observations_;
    std::uint64_t overflow_count_ = 0;
};

[[nodiscard]] std::string metric_kind_name(MetricKind kind);
[[nodiscard]] std::optional<MetricKind> parse_metric_kind(std::string_view value);
[[nodiscard]] std::string metric_trend_name(MetricTrend trend);
[[nodiscard]] std::string canonical_metric_key(const MetricIdentity& identity);
[[nodiscard]] bool same_metric_identity(const MetricIdentity& left, const MetricIdentity& right);
[[nodiscard]] MetricIdentity make_metric_identity(std::string name, MetricTags tags = {});
[[nodiscard]] MetricTags merge_metric_tags(MetricTags base, const MetricTags& overlay);
[[nodiscard]] std::vector<std::string> metric_tag_keys(const MetricTags& tags);
[[nodiscard]] std::vector<MetricRollup> rollup_series(const std::vector<MetricSeries>& series);
[[nodiscard]] MetricRollup rollup_samples(const MetricIdentity& identity,
                                          MetricKind kind,
                                          std::string unit,
                                          const std::vector<MetricSample>& samples);
[[nodiscard]] std::vector<MetricSeries> group_metric_series(const std::vector<MetricSample>& samples);
[[nodiscard]] std::vector<MetricDelta> compare_metric_snapshots(const MetricSnapshot& previous,
                                                                const MetricSnapshot& current);
[[nodiscard]] MetricTrend classify_metric_trend(const std::vector<MetricSample>& samples,
                                                double noise_floor = 0.0);
[[nodiscard]] std::vector<MetricSample> filter_metrics_by_time(const std::vector<MetricSample>& samples,
                                                               std::uint64_t start_ns,
                                                               std::uint64_t end_ns);
[[nodiscard]] std::vector<MetricSample> filter_metrics_by_name_prefix(const std::vector<MetricSample>& samples,
                                                                      std::string_view prefix);
[[nodiscard]] double percentile(std::vector<double> values, double percentile_rank);
[[nodiscard]] std::string format_metric_value(double value, std::string_view unit);
[[nodiscard]] std::string render_metric_snapshot_text(const MetricSnapshot& snapshot);
[[nodiscard]] std::string render_metric_snapshot_json(const MetricSnapshot& snapshot);
[[nodiscard]] std::string render_metric_rollups_text(const std::vector<MetricRollup>& rollups);
[[nodiscard]] std::string render_metric_deltas_text(const std::vector<MetricDelta>& deltas);
[[nodiscard]] std::string render_latency_histogram_text(const LatencyHistogram& histogram);
[[nodiscard]] std::string render_latency_histogram_json(const LatencyHistogram& histogram);
[[nodiscard]] std::vector<std::uint64_t> default_latency_buckets_ns();

} // namespace aethon::diagnostics
