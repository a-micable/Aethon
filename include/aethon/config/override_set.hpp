#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::config {

struct OverrideSetSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct OverrideSetSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class OverrideSet {
public:
    explicit OverrideSet(std::string name = "override_set");
    void observe(OverrideSetSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] OverrideSetSummary summarize() const;
    [[nodiscard]] std::optional<OverrideSetSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<OverrideSetSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<OverrideSetSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

OverrideSetSummary summarize_override_set(const std::vector<OverrideSetSample>& samples);
double override_set_stability_index(const OverrideSetSummary& summary);
std::string describe_override_set(const OverrideSetSummary& summary);

} // namespace aethon::config
