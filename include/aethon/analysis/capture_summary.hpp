#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct CaptureSummarySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CaptureSummarySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CaptureSummary {
public:
    explicit CaptureSummary(std::string name = "capture_summary");
    void observe(CaptureSummarySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CaptureSummarySummary summarize() const;
    [[nodiscard]] std::optional<CaptureSummarySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CaptureSummarySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CaptureSummarySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CaptureSummarySummary summarize_capture_summary(const std::vector<CaptureSummarySample>& samples);
double capture_summary_stability_index(const CaptureSummarySummary& summary);
std::string describe_capture_summary(const CaptureSummarySummary& summary);

} // namespace aethon::analysis
