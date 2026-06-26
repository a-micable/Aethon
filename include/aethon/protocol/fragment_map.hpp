#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::protocol {

struct FragmentMapSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct FragmentMapSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class FragmentMap {
public:
    explicit FragmentMap(std::string name = "fragment_map");
    void observe(FragmentMapSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] FragmentMapSummary summarize() const;
    [[nodiscard]] std::optional<FragmentMapSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<FragmentMapSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<FragmentMapSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

FragmentMapSummary summarize_fragment_map(const std::vector<FragmentMapSample>& samples);
double fragment_map_stability_index(const FragmentMapSummary& summary);
std::string describe_fragment_map(const FragmentMapSummary& summary);

} // namespace aethon::protocol
