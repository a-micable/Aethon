#include "aethon/diagnostics/metrics.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

constexpr double relative_epsilon = 0.000001;

bool almost_equal(double left, double right, double epsilon) {
    return std::abs(left - right) <= epsilon;
}

double safe_relative_change(double previous, double current) {
    if (std::abs(previous) < relative_epsilon) {
        return current == 0.0 ? 0.0 : 1.0;
    }
    return (current - previous) / std::abs(previous);
}

std::string format_double(double value, int precision = 3) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string render_tags(const MetricTags& tags) {
    if (tags.empty()) {
        return "";
    }
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

JsonObject sample_to_json(const MetricSample& sample) {
    JsonObject object;
    object.string("name", sample.identity.name);
    object.string("kind", metric_kind_name(sample.kind));
    object.raw("tags", tags_json(sample.identity.tags));
    object.raw("value", format_double(sample.value, 6));
    object.number("time_ns", sample.time_ns);
    object.string("unit", sample.unit);
    if (!sample.description.empty()) {
        object.string("description", sample.description);
    }
    return object;
}

JsonObject bucket_to_json(const LatencyBucket& bucket) {
    JsonObject object;
    object.number("upper_bound_ns", bucket.upper_bound_ns);
    object.number("count", bucket.count);
    return object;
}

std::string buckets_json(const std::vector<LatencyBucket>& buckets) {
    std::vector<JsonObject> objects;
    objects.reserve(buckets.size());
    for (const auto& bucket : buckets) {
        objects.push_back(bucket_to_json(bucket));
    }
    return render_json_array(objects);
}

void update_snapshot_capture_time(std::uint64_t sample_time, std::uint64_t& capture_time) {
    capture_time = std::max(capture_time, sample_time);
}

void count_kind(MetricKind kind, MetricSnapshotSummary& summary) {
    switch (kind) {
    case MetricKind::counter:
        ++summary.counter_count;
        break;
    case MetricKind::gauge:
        ++summary.gauge_count;
        break;
    case MetricKind::latency:
        ++summary.latency_count;
        break;
    case MetricKind::ratio:
        ++summary.ratio_count;
        break;
    case MetricKind::bytes:
        ++summary.bytes_count;
        break;
    }
}

std::optional<MetricSample> latest_sample_for(const std::vector<MetricSample>& samples,
                                              const MetricIdentity& identity) {
    std::optional<MetricSample> found;
    for (const auto& sample : samples) {
        if (!same_metric_identity(sample.identity, identity)) {
            continue;
        }
        if (!found || sample.time_ns >= found->time_ns) {
            found = sample;
        }
    }
    return found;
}

void sort_samples_by_time(std::vector<MetricSample>& samples) {
    std::sort(
        samples.begin(),
        samples.end(),
        [](const MetricSample& left, const MetricSample& right) {
            if (left.time_ns != right.time_ns) {
                return left.time_ns < right.time_ns;
            }
            return canonical_metric_key(left.identity) < canonical_metric_key(right.identity);
        });
}

std::vector<std::uint64_t> sorted_latency_values(const std::vector<LatencyObservation>& observations) {
    std::vector<std::uint64_t> values;
    values.reserve(observations.size());
    for (const auto& observation : observations) {
        values.push_back(observation.value_ns);
    }
    std::sort(values.begin(), values.end());
    return values;
}

double latency_percentile(const std::vector<std::uint64_t>& sorted_values, double percentile_rank) {
    if (sorted_values.empty()) {
        return 0.0;
    }
    if (percentile_rank <= 0.0) {
        return static_cast<double>(sorted_values.front());
    }
    if (percentile_rank >= 100.0) {
        return static_cast<double>(sorted_values.back());
    }
    const double position = (percentile_rank / 100.0) * static_cast<double>(sorted_values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) {
        return static_cast<double>(sorted_values[lower]);
    }
    const double weight = position - static_cast<double>(lower);
    return static_cast<double>(sorted_values[lower]) * (1.0 - weight)
        + static_cast<double>(sorted_values[upper]) * weight;
}

} // namespace

bool MetricIdentity::empty() const noexcept {
    return name.empty() && tags.empty();
}

bool MetricSeries::empty() const noexcept {
    return samples.empty();
}

std::size_t MetricSeries::size() const noexcept {
    return samples.size();
}

void MetricSnapshot::add(MetricSample sample) {
    update_snapshot_capture_time(sample.time_ns, capture_time_ns_);
    samples_.push_back(std::move(sample));
}

void MetricSnapshot::clear() {
    samples_.clear();
    capture_time_ns_ = 0;
}

bool MetricSnapshot::empty() const noexcept {
    return samples_.empty();
}

std::size_t MetricSnapshot::size() const noexcept {
    return samples_.size();
}

std::uint64_t MetricSnapshot::capture_time_ns() const noexcept {
    return capture_time_ns_;
}

const std::vector<MetricSample>& MetricSnapshot::samples() const noexcept {
    return samples_;
}

std::vector<MetricSample> MetricSnapshot::find_by_name(std::string_view name) const {
    std::vector<MetricSample> found;
    for (const auto& sample : samples_) {
        if (sample.identity.name == name) {
            found.push_back(sample);
        }
    }
    sort_samples_by_time(found);
    return found;
}

std::vector<MetricSample> MetricSnapshot::find_by_tag(std::string_view key,
                                                      std::string_view value) const {
    std::vector<MetricSample> found;
    for (const auto& sample : samples_) {
        auto it = sample.identity.tags.find(std::string(key));
        if (it != sample.identity.tags.end() && it->second == value) {
            found.push_back(sample);
        }
    }
    sort_samples_by_time(found);
    return found;
}

std::optional<MetricSample> MetricSnapshot::latest(std::string_view name) const {
    std::optional<MetricSample> found;
    for (const auto& sample : samples_) {
        if (sample.identity.name != name) {
            continue;
        }
        if (!found || sample.time_ns >= found->time_ns) {
            found = sample;
        }
    }
    return found;
}

std::optional<MetricSample> MetricSnapshot::latest(const MetricIdentity& identity) const {
    return latest_sample_for(samples_, identity);
}

MetricSnapshotSummary MetricSnapshot::summary() const {
    MetricSnapshotSummary summary;
    summary.capture_time_ns = capture_time_ns_;
    summary.metric_count = samples_.size();
    for (const auto& sample : samples_) {
        count_kind(sample.kind, summary);
        if (sample.kind == MetricKind::counter) {
            summary.total_counter_value += sample.value;
        }
        if (sample.kind == MetricKind::bytes) {
            summary.total_byte_value += sample.value;
        }
    }
    return summary;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::capture_time(std::uint64_t time_ns) {
    capture_time_ns_ = time_ns;
    return *this;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::counter(std::string name,
                                                       double value,
                                                       MetricTags tags,
                                                       std::string unit) {
    snapshot_.add(MetricSample{
        make_metric_identity(std::move(name), std::move(tags)),
        MetricKind::counter,
        value,
        capture_time_ns_,
        std::move(unit),
        {},
    });
    return *this;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::gauge(std::string name,
                                                     double value,
                                                     MetricTags tags,
                                                     std::string unit) {
    snapshot_.add(MetricSample{
        make_metric_identity(std::move(name), std::move(tags)),
        MetricKind::gauge,
        value,
        capture_time_ns_,
        std::move(unit),
        {},
    });
    return *this;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::latency(std::string name,
                                                       double value_ns,
                                                       MetricTags tags) {
    snapshot_.add(MetricSample{
        make_metric_identity(std::move(name), std::move(tags)),
        MetricKind::latency,
        value_ns,
        capture_time_ns_,
        "ns",
        {},
    });
    return *this;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::ratio(std::string name,
                                                     double value,
                                                     MetricTags tags) {
    snapshot_.add(MetricSample{
        make_metric_identity(std::move(name), std::move(tags)),
        MetricKind::ratio,
        value,
        capture_time_ns_,
        "ratio",
        {},
    });
    return *this;
}

MetricSnapshotBuilder& MetricSnapshotBuilder::bytes(std::string name,
                                                     double value,
                                                     MetricTags tags) {
    snapshot_.add(MetricSample{
        make_metric_identity(std::move(name), std::move(tags)),
        MetricKind::bytes,
        value,
        capture_time_ns_,
        "bytes",
        {},
    });
    return *this;
}

MetricSnapshot MetricSnapshotBuilder::build() const {
    return snapshot_;
}

void MetricSnapshotBuilder::reset() {
    snapshot_.clear();
    capture_time_ns_ = 0;
}

LatencyHistogram::LatencyHistogram()
    : LatencyHistogram(default_latency_buckets_ns()) {
}

LatencyHistogram::LatencyHistogram(std::vector<std::uint64_t> bucket_bounds_ns) {
    std::sort(bucket_bounds_ns.begin(), bucket_bounds_ns.end());
    bucket_bounds_ns.erase(
        std::unique(bucket_bounds_ns.begin(), bucket_bounds_ns.end()),
        bucket_bounds_ns.end());
    buckets_.reserve(bucket_bounds_ns.size());
    for (auto bound : bucket_bounds_ns) {
        buckets_.push_back(LatencyBucket{bound, 0});
    }
}

void LatencyHistogram::observe(std::uint64_t value_ns, std::uint64_t time_ns, MetricTags tags) {
    observations_.push_back(LatencyObservation{
        value_ns,
        time_ns,
        std::move(tags),
    });
    auto it = std::lower_bound(
        buckets_.begin(),
        buckets_.end(),
        value_ns,
        [](const LatencyBucket& bucket, std::uint64_t value) {
            return bucket.upper_bound_ns < value;
        });
    if (it == buckets_.end()) {
        ++overflow_count_;
        return;
    }
    ++it->count;
}

void LatencyHistogram::merge(const LatencyHistogram& other) {
    for (const auto& observation : other.observations()) {
        observe(observation.value_ns, observation.time_ns, observation.tags);
    }
}

void LatencyHistogram::clear() {
    for (auto& bucket : buckets_) {
        bucket.count = 0;
    }
    observations_.clear();
    overflow_count_ = 0;
}

bool LatencyHistogram::empty() const noexcept {
    return observations_.empty();
}

std::uint64_t LatencyHistogram::count() const noexcept {
    return static_cast<std::uint64_t>(observations_.size());
}

std::uint64_t LatencyHistogram::overflow_count() const noexcept {
    return overflow_count_;
}

const std::vector<LatencyBucket>& LatencyHistogram::buckets() const noexcept {
    return buckets_;
}

const std::vector<LatencyObservation>& LatencyHistogram::observations() const noexcept {
    return observations_;
}

LatencyHistogramStats LatencyHistogram::stats() const {
    LatencyHistogramStats stats;
    stats.count = count();
    stats.overflow_count = overflow_count_;
    if (observations_.empty()) {
        return stats;
    }
    auto sorted = sorted_latency_values(observations_);
    stats.min_ns = sorted.front();
    stats.max_ns = sorted.back();
    stats.sum_ns = std::accumulate(sorted.begin(), sorted.end(), std::uint64_t{0});
    stats.mean_ns = static_cast<double>(stats.sum_ns) / static_cast<double>(stats.count);
    stats.p50_ns = latency_percentile(sorted, 50.0);
    stats.p90_ns = latency_percentile(sorted, 90.0);
    stats.p95_ns = latency_percentile(sorted, 95.0);
    stats.p99_ns = latency_percentile(sorted, 99.0);
    return stats;
}

std::vector<LatencyObservation> LatencyHistogram::observations_between(std::uint64_t start_ns,
                                                                       std::uint64_t end_ns) const {
    std::vector<LatencyObservation> selected;
    for (const auto& observation : observations_) {
        if (observation.time_ns >= start_ns && observation.time_ns <= end_ns) {
            selected.push_back(observation);
        }
    }
    return selected;
}

std::vector<LatencyObservation> LatencyHistogram::observations_with_tag(std::string_view key,
                                                                       std::string_view value) const {
    std::vector<LatencyObservation> selected;
    for (const auto& observation : observations_) {
        auto it = observation.tags.find(std::string(key));
        if (it != observation.tags.end() && it->second == value) {
            selected.push_back(observation);
        }
    }
    return selected;
}

std::string metric_kind_name(MetricKind kind) {
    switch (kind) {
    case MetricKind::counter:
        return "counter";
    case MetricKind::gauge:
        return "gauge";
    case MetricKind::latency:
        return "latency";
    case MetricKind::ratio:
        return "ratio";
    case MetricKind::bytes:
        return "bytes";
    }
    return "unknown";
}

std::optional<MetricKind> parse_metric_kind(std::string_view value) {
    if (value == "counter") {
        return MetricKind::counter;
    }
    if (value == "gauge") {
        return MetricKind::gauge;
    }
    if (value == "latency") {
        return MetricKind::latency;
    }
    if (value == "ratio") {
        return MetricKind::ratio;
    }
    if (value == "bytes") {
        return MetricKind::bytes;
    }
    return std::nullopt;
}

std::string metric_trend_name(MetricTrend trend) {
    switch (trend) {
    case MetricTrend::missing:
        return "missing";
    case MetricTrend::flat:
        return "flat";
    case MetricTrend::rising:
        return "rising";
    case MetricTrend::falling:
        return "falling";
    case MetricTrend::volatile_value:
        return "volatile";
    }
    return "unknown";
}

std::string canonical_metric_key(const MetricIdentity& identity) {
    std::ostringstream out;
    out << identity.name;
    for (const auto& [key, value] : identity.tags) {
        out << "|" << key << "=" << value;
    }
    return out.str();
}

bool same_metric_identity(const MetricIdentity& left, const MetricIdentity& right) {
    return left.name == right.name && left.tags == right.tags;
}

MetricIdentity make_metric_identity(std::string name, MetricTags tags) {
    return MetricIdentity{
        std::move(name),
        std::move(tags),
    };
}

MetricTags merge_metric_tags(MetricTags base, const MetricTags& overlay) {
    for (const auto& [key, value] : overlay) {
        base[key] = value;
    }
    return base;
}

std::vector<std::string> metric_tag_keys(const MetricTags& tags) {
    std::vector<std::string> keys;
    keys.reserve(tags.size());
    for (const auto& [key, value] : tags) {
        (void)value;
        keys.push_back(key);
    }
    return keys;
}

std::vector<MetricRollup> rollup_series(const std::vector<MetricSeries>& series) {
    std::vector<MetricRollup> rollups;
    rollups.reserve(series.size());
    for (const auto& item : series) {
        rollups.push_back(rollup_samples(item.identity, item.kind, item.unit, item.samples));
    }
    std::sort(
        rollups.begin(),
        rollups.end(),
        [](const MetricRollup& left, const MetricRollup& right) {
            return canonical_metric_key(left.identity) < canonical_metric_key(right.identity);
        });
    return rollups;
}

MetricRollup rollup_samples(const MetricIdentity& identity,
                            MetricKind kind,
                            std::string unit,
                            const std::vector<MetricSample>& samples) {
    MetricRollup rollup;
    rollup.identity = identity;
    rollup.kind = kind;
    rollup.unit = std::move(unit);
    rollup.sample_count = samples.size();
    if (samples.empty()) {
        return rollup;
    }

    auto ordered = samples;
    sort_samples_by_time(ordered);
    rollup.first_time_ns = ordered.front().time_ns;
    rollup.last_time_ns = ordered.back().time_ns;
    rollup.min = ordered.front().value;
    rollup.max = ordered.front().value;
    rollup.latest = ordered.back().value;
    rollup.delta = ordered.back().value - ordered.front().value;
    for (const auto& sample : ordered) {
        rollup.min = std::min(rollup.min, sample.value);
        rollup.max = std::max(rollup.max, sample.value);
        rollup.sum += sample.value;
    }
    rollup.average = rollup.sum / static_cast<double>(ordered.size());
    return rollup;
}

std::vector<MetricSeries> group_metric_series(const std::vector<MetricSample>& samples) {
    std::map<std::string, MetricSeries> grouped;
    for (const auto& sample : samples) {
        auto key = canonical_metric_key(sample.identity);
        auto& series = grouped[key];
        if (series.identity.empty()) {
            series.identity = sample.identity;
            series.kind = sample.kind;
            series.unit = sample.unit;
        }
        series.samples.push_back(sample);
    }

    std::vector<MetricSeries> series;
    series.reserve(grouped.size());
    for (auto& [key, item] : grouped) {
        (void)key;
        sort_samples_by_time(item.samples);
        series.push_back(std::move(item));
    }
    return series;
}

std::vector<MetricDelta> compare_metric_snapshots(const MetricSnapshot& previous,
                                                  const MetricSnapshot& current) {
    std::map<std::string, MetricDelta> deltas;
    for (const auto& sample : previous.samples()) {
        auto key = canonical_metric_key(sample.identity);
        auto& delta = deltas[key];
        delta.identity = sample.identity;
        delta.kind = sample.kind;
        delta.unit = sample.unit;
        delta.previous = sample.value;
    }
    for (const auto& sample : current.samples()) {
        auto key = canonical_metric_key(sample.identity);
        auto& delta = deltas[key];
        delta.identity = sample.identity;
        delta.kind = sample.kind;
        delta.unit = sample.unit;
        delta.current = sample.value;
    }

    std::vector<MetricDelta> results;
    results.reserve(deltas.size());
    for (auto& [key, delta] : deltas) {
        (void)key;
        if (delta.previous && delta.current) {
            delta.absolute_change = *delta.current - *delta.previous;
            delta.relative_change = safe_relative_change(*delta.previous, *delta.current);
            if (almost_equal(delta.absolute_change, 0.0, relative_epsilon)) {
                delta.trend = MetricTrend::flat;
            } else if (delta.absolute_change > 0.0) {
                delta.trend = MetricTrend::rising;
            } else {
                delta.trend = MetricTrend::falling;
            }
        } else {
            delta.trend = MetricTrend::missing;
        }
        results.push_back(std::move(delta));
    }
    return results;
}

MetricTrend classify_metric_trend(const std::vector<MetricSample>& samples, double noise_floor) {
    if (samples.size() < 2) {
        return MetricTrend::missing;
    }
    auto ordered = samples;
    sort_samples_by_time(ordered);
    std::size_t rising = 0;
    std::size_t falling = 0;
    std::size_t flat = 0;
    for (std::size_t i = 1; i < ordered.size(); ++i) {
        const double diff = ordered[i].value - ordered[i - 1].value;
        if (std::abs(diff) <= noise_floor) {
            ++flat;
        } else if (diff > 0.0) {
            ++rising;
        } else {
            ++falling;
        }
    }
    if (flat == ordered.size() - 1) {
        return MetricTrend::flat;
    }
    if (rising > 0 && falling > 0) {
        return MetricTrend::volatile_value;
    }
    return rising > falling ? MetricTrend::rising : MetricTrend::falling;
}

std::vector<MetricSample> filter_metrics_by_time(const std::vector<MetricSample>& samples,
                                                 std::uint64_t start_ns,
                                                 std::uint64_t end_ns) {
    std::vector<MetricSample> selected;
    for (const auto& sample : samples) {
        if (sample.time_ns >= start_ns && sample.time_ns <= end_ns) {
            selected.push_back(sample);
        }
    }
    sort_samples_by_time(selected);
    return selected;
}

std::vector<MetricSample> filter_metrics_by_name_prefix(const std::vector<MetricSample>& samples,
                                                        std::string_view prefix) {
    std::vector<MetricSample> selected;
    for (const auto& sample : samples) {
        if (sample.identity.name.rfind(prefix, 0) == 0) {
            selected.push_back(sample);
        }
    }
    sort_samples_by_time(selected);
    return selected;
}

double percentile(std::vector<double> values, double percentile_rank) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (percentile_rank <= 0.0) {
        return values.front();
    }
    if (percentile_rank >= 100.0) {
        return values.back();
    }
    const double position = (percentile_rank / 100.0) * static_cast<double>(values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) {
        return values[lower];
    }
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

std::string format_metric_value(double value, std::string_view unit) {
    std::ostringstream out;
    if (unit == "ratio") {
        out << std::fixed << std::setprecision(2) << value * 100.0 << "%";
        return out.str();
    }
    if (unit == "ns") {
        if (value >= 1000000.0) {
            out << std::fixed << std::setprecision(3) << value / 1000000.0 << "ms";
        } else if (value >= 1000.0) {
            out << std::fixed << std::setprecision(3) << value / 1000.0 << "us";
        } else {
            out << std::fixed << std::setprecision(0) << value << "ns";
        }
        return out.str();
    }
    if (unit == "bytes") {
        if (value >= 1024.0 * 1024.0) {
            out << std::fixed << std::setprecision(2) << value / (1024.0 * 1024.0) << "MiB";
        } else if (value >= 1024.0) {
            out << std::fixed << std::setprecision(2) << value / 1024.0 << "KiB";
        } else {
            out << std::fixed << std::setprecision(0) << value << "B";
        }
        return out.str();
    }
    out << std::fixed << std::setprecision(3) << value;
    if (!unit.empty()) {
        out << unit;
    }
    return out.str();
}

std::string render_metric_snapshot_text(const MetricSnapshot& snapshot) {
    TextTable table;
    table.headers = {"metric", "kind", "tags", "value", "time_ns"};
    for (const auto& sample : snapshot.samples()) {
        add_row(
            table,
            {
                sample.identity.name,
                metric_kind_name(sample.kind),
                render_tags(sample.identity.tags),
                format_metric_value(sample.value, sample.unit),
                std::to_string(sample.time_ns),
            });
    }
    return render_text_table(table);
}

std::string render_metric_snapshot_json(const MetricSnapshot& snapshot) {
    std::vector<JsonObject> samples;
    samples.reserve(snapshot.samples().size());
    for (const auto& sample : snapshot.samples()) {
        samples.push_back(sample_to_json(sample));
    }
    auto summary = snapshot.summary();
    JsonObject root;
    root.number("capture_time_ns", snapshot.capture_time_ns());
    root.number("metric_count", static_cast<std::uint64_t>(snapshot.size()));
    root.number("counter_count", static_cast<std::uint64_t>(summary.counter_count));
    root.number("gauge_count", static_cast<std::uint64_t>(summary.gauge_count));
    root.number("latency_count", static_cast<std::uint64_t>(summary.latency_count));
    root.number("ratio_count", static_cast<std::uint64_t>(summary.ratio_count));
    root.number("bytes_count", static_cast<std::uint64_t>(summary.bytes_count));
    root.raw("samples", render_json_array(samples));
    return render_json_object(root);
}

std::string render_metric_rollups_text(const std::vector<MetricRollup>& rollups) {
    TextTable table;
    table.headers = {"metric", "kind", "samples", "min", "max", "avg", "latest", "delta"};
    for (const auto& rollup : rollups) {
        add_row(
            table,
            {
                canonical_metric_key(rollup.identity),
                metric_kind_name(rollup.kind),
                std::to_string(rollup.sample_count),
                format_metric_value(rollup.min, rollup.unit),
                format_metric_value(rollup.max, rollup.unit),
                format_metric_value(rollup.average, rollup.unit),
                format_metric_value(rollup.latest, rollup.unit),
                format_metric_value(rollup.delta, rollup.unit),
            });
    }
    return render_text_table(table);
}

std::string render_metric_deltas_text(const std::vector<MetricDelta>& deltas) {
    TextTable table;
    table.headers = {"metric", "kind", "previous", "current", "change", "trend"};
    for (const auto& delta : deltas) {
        add_row(
            table,
            {
                canonical_metric_key(delta.identity),
                metric_kind_name(delta.kind),
                delta.previous ? format_metric_value(*delta.previous, delta.unit) : "-",
                delta.current ? format_metric_value(*delta.current, delta.unit) : "-",
                format_metric_value(delta.absolute_change, delta.unit),
                metric_trend_name(delta.trend),
            });
    }
    return render_text_table(table);
}

std::string render_latency_histogram_text(const LatencyHistogram& histogram) {
    const auto stats = histogram.stats();
    TextTable summary;
    summary.headers = {"count", "min", "mean", "p50", "p90", "p95", "p99", "max", "overflow"};
    add_row(
        summary,
        {
            std::to_string(stats.count),
            format_metric_value(static_cast<double>(stats.min_ns), "ns"),
            format_metric_value(stats.mean_ns, "ns"),
            format_metric_value(stats.p50_ns, "ns"),
            format_metric_value(stats.p90_ns, "ns"),
            format_metric_value(stats.p95_ns, "ns"),
            format_metric_value(stats.p99_ns, "ns"),
            format_metric_value(static_cast<double>(stats.max_ns), "ns"),
            std::to_string(stats.overflow_count),
        });

    TextTable buckets;
    buckets.headers = {"le_ns", "count"};
    for (const auto& bucket : histogram.buckets()) {
        add_row(
            buckets,
            {
                std::to_string(bucket.upper_bound_ns),
                std::to_string(bucket.count),
            });
    }
    std::ostringstream out;
    out << "latency_summary\n"
        << render_text_table(summary)
        << "latency_buckets\n"
        << render_text_table(buckets);
    return out.str();
}

std::string render_latency_histogram_json(const LatencyHistogram& histogram) {
    const auto stats = histogram.stats();
    JsonObject summary;
    summary.number("count", stats.count);
    summary.number("min_ns", stats.min_ns);
    summary.number("max_ns", stats.max_ns);
    summary.number("sum_ns", stats.sum_ns);
    summary.raw("mean_ns", format_double(stats.mean_ns, 6));
    summary.raw("p50_ns", format_double(stats.p50_ns, 6));
    summary.raw("p90_ns", format_double(stats.p90_ns, 6));
    summary.raw("p95_ns", format_double(stats.p95_ns, 6));
    summary.raw("p99_ns", format_double(stats.p99_ns, 6));
    summary.number("overflow_count", stats.overflow_count);

    JsonObject root;
    root.raw("summary", render_json_object(summary));
    root.raw("buckets", buckets_json(histogram.buckets()));
    return render_json_object(root);
}

std::vector<std::uint64_t> default_latency_buckets_ns() {
    return {
        1000,
        2500,
        5000,
        10000,
        25000,
        50000,
        100000,
        250000,
        500000,
        1000000,
        2500000,
        5000000,
        10000000,
        25000000,
        50000000,
        100000000,
        250000000,
        500000000,
        1000000000,
    };
}

} // namespace aethon::diagnostics
