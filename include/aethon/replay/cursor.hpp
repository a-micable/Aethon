#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::replay {

struct ReplayCursorSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ReplayCursorSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ReplayCursor {
public:
    explicit ReplayCursor(std::string name = "cursor");
    void observe(ReplayCursorSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ReplayCursorSummary summarize() const;
    [[nodiscard]] std::optional<ReplayCursorSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ReplayCursorSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ReplayCursorSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ReplayCursorSummary summarize_cursor(const std::vector<ReplayCursorSample>& samples);
double cursor_stability_index(const ReplayCursorSummary& summary);
std::string describe_cursor(const ReplayCursorSummary& summary);

} // namespace aethon::replay
