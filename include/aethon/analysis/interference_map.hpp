#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct InterferenceMapSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct InterferenceMapSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class InterferenceMap {
public:
    explicit InterferenceMap(std::string name = "interference_map");
    void observe(InterferenceMapSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] InterferenceMapSummary summarize() const;
    [[nodiscard]] std::optional<InterferenceMapSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<InterferenceMapSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<InterferenceMapSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

InterferenceMapSummary summarize_interference_map(const std::vector<InterferenceMapSample>& samples);
double interference_map_stability_index(const InterferenceMapSummary& summary);
std::string describe_interference_map(const InterferenceMapSummary& summary);

} // namespace aethon::analysis
