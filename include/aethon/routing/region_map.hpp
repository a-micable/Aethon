#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::routing {

struct RegionMapSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RegionMapSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RegionMap {
public:
    explicit RegionMap(std::string name = "region_map");
    void observe(RegionMapSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RegionMapSummary summarize() const;
    [[nodiscard]] std::optional<RegionMapSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RegionMapSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RegionMapSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RegionMapSummary summarize_region_map(const std::vector<RegionMapSample>& samples);
double region_map_stability_index(const RegionMapSummary& summary);
std::string describe_region_map(const RegionMapSummary& summary);

} // namespace aethon::routing
