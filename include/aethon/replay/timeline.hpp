#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::replay {

struct ReplayTimelineSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ReplayTimelineSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ReplayTimeline {
public:
    explicit ReplayTimeline(std::string name = "timeline");
    void observe(ReplayTimelineSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ReplayTimelineSummary summarize() const;
    [[nodiscard]] std::optional<ReplayTimelineSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ReplayTimelineSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ReplayTimelineSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ReplayTimelineSummary summarize_timeline(const std::vector<ReplayTimelineSample>& samples);
double timeline_stability_index(const ReplayTimelineSummary& summary);
std::string describe_timeline(const ReplayTimelineSummary& summary);

} // namespace aethon::replay
