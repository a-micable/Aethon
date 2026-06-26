#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::integrity {

struct SequenceTrackerSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SequenceTrackerSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SequenceTracker {
public:
    explicit SequenceTracker(std::string name = "sequence_tracker");
    void observe(SequenceTrackerSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SequenceTrackerSummary summarize() const;
    [[nodiscard]] std::optional<SequenceTrackerSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SequenceTrackerSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SequenceTrackerSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SequenceTrackerSummary summarize_sequence_tracker(const std::vector<SequenceTrackerSample>& samples);
double sequence_tracker_stability_index(const SequenceTrackerSummary& summary);
std::string describe_sequence_tracker(const SequenceTrackerSummary& summary);

} // namespace aethon::integrity
