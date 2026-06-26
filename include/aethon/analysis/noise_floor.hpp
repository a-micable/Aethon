#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct NoiseFloorSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct NoiseFloorSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class NoiseFloor {
public:
    explicit NoiseFloor(std::string name = "noise_floor");
    void observe(NoiseFloorSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] NoiseFloorSummary summarize() const;
    [[nodiscard]] std::optional<NoiseFloorSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<NoiseFloorSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<NoiseFloorSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

NoiseFloorSummary summarize_noise_floor(const std::vector<NoiseFloorSample>& samples);
double noise_floor_stability_index(const NoiseFloorSummary& summary);
std::string describe_noise_floor(const NoiseFloorSummary& summary);

} // namespace aethon::analysis
