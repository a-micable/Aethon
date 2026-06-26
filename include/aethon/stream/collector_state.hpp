#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct CollectorStateSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CollectorStateSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CollectorState {
public:
    explicit CollectorState(std::string name = "collector_state");
    void observe(CollectorStateSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CollectorStateSummary summarize() const;
    [[nodiscard]] std::optional<CollectorStateSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CollectorStateSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CollectorStateSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CollectorStateSummary summarize_collector_state(const std::vector<CollectorStateSample>& samples);
double collector_state_stability_index(const CollectorStateSummary& summary);
std::string describe_collector_state(const CollectorStateSummary& summary);

} // namespace aethon::stream
