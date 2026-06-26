#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct BandPlanSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct BandPlanSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class BandPlan {
public:
    explicit BandPlan(std::string name = "band_plan");
    void observe(BandPlanSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] BandPlanSummary summarize() const;
    [[nodiscard]] std::optional<BandPlanSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<BandPlanSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<BandPlanSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

BandPlanSummary summarize_band_plan(const std::vector<BandPlanSample>& samples);
double band_plan_stability_index(const BandPlanSummary& summary);
std::string describe_band_plan(const BandPlanSummary& summary);

} // namespace aethon::analysis
