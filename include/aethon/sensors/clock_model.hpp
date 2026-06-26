#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::sensors {

struct ClockModelSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ClockModelSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ClockModel {
public:
    explicit ClockModel(std::string name = "clock_model");
    void observe(ClockModelSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ClockModelSummary summarize() const;
    [[nodiscard]] std::optional<ClockModelSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ClockModelSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ClockModelSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ClockModelSummary summarize_clock_model(const std::vector<ClockModelSample>& samples);
double clock_model_stability_index(const ClockModelSummary& summary);
std::string describe_clock_model(const ClockModelSummary& summary);

} // namespace aethon::sensors
