#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct MetricSnapshotSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct MetricSnapshotSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class MetricSnapshot {
public:
    explicit MetricSnapshot(std::string name = "metric_snapshot");
    void observe(MetricSnapshotSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] MetricSnapshotSummary summarize() const;
    [[nodiscard]] std::optional<MetricSnapshotSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<MetricSnapshotSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<MetricSnapshotSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

MetricSnapshotSummary summarize_metric_snapshot(const std::vector<MetricSnapshotSample>& samples);
double metric_snapshot_stability_index(const MetricSnapshotSummary& summary);
std::string describe_metric_snapshot(const MetricSnapshotSummary& summary);

} // namespace aethon::diagnostics
