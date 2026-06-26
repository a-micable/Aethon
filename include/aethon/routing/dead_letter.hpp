#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::routing {

struct DeadLetterQueueSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct DeadLetterQueueSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class DeadLetterQueue {
public:
    explicit DeadLetterQueue(std::string name = "dead_letter");
    void observe(DeadLetterQueueSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] DeadLetterQueueSummary summarize() const;
    [[nodiscard]] std::optional<DeadLetterQueueSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<DeadLetterQueueSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<DeadLetterQueueSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

DeadLetterQueueSummary summarize_dead_letter(const std::vector<DeadLetterQueueSample>& samples);
double dead_letter_stability_index(const DeadLetterQueueSummary& summary);
std::string describe_dead_letter(const DeadLetterQueueSummary& summary);

} // namespace aethon::routing
