#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::routing {

struct PriorityQueueSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct PriorityQueueSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class PriorityQueue {
public:
    explicit PriorityQueue(std::string name = "priority_queue");
    void observe(PriorityQueueSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] PriorityQueueSummary summarize() const;
    [[nodiscard]] std::optional<PriorityQueueSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<PriorityQueueSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<PriorityQueueSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

PriorityQueueSummary summarize_priority_queue(const std::vector<PriorityQueueSample>& samples);
double priority_queue_stability_index(const PriorityQueueSummary& summary);
std::string describe_priority_queue(const PriorityQueueSummary& summary);

} // namespace aethon::routing
