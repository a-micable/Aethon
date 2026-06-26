#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::stream {

struct BackpressureControllerSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct BackpressureControllerSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class BackpressureController {
public:
    explicit BackpressureController(std::string name = "backpressure");
    void observe(BackpressureControllerSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] BackpressureControllerSummary summarize() const;
    [[nodiscard]] std::optional<BackpressureControllerSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<BackpressureControllerSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<BackpressureControllerSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

BackpressureControllerSummary summarize_backpressure(const std::vector<BackpressureControllerSample>& samples);
double backpressure_stability_index(const BackpressureControllerSummary& summary);
std::string describe_backpressure(const BackpressureControllerSummary& summary);

} // namespace aethon::stream
