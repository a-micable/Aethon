#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::replay {

struct SpeedControllerSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SpeedControllerSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SpeedController {
public:
    explicit SpeedController(std::string name = "speed_controller");
    void observe(SpeedControllerSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SpeedControllerSummary summarize() const;
    [[nodiscard]] std::optional<SpeedControllerSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SpeedControllerSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SpeedControllerSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SpeedControllerSummary summarize_speed_controller(const std::vector<SpeedControllerSample>& samples);
double speed_controller_stability_index(const SpeedControllerSummary& summary);
std::string describe_speed_controller(const SpeedControllerSummary& summary);

} // namespace aethon::replay
