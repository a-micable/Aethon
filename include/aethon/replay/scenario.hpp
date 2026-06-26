#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::replay {

struct ReplayScenarioSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ReplayScenarioSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ReplayScenario {
public:
    explicit ReplayScenario(std::string name = "scenario");
    void observe(ReplayScenarioSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ReplayScenarioSummary summarize() const;
    [[nodiscard]] std::optional<ReplayScenarioSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ReplayScenarioSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ReplayScenarioSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ReplayScenarioSummary summarize_scenario(const std::vector<ReplayScenarioSample>& samples);
double scenario_stability_index(const ReplayScenarioSummary& summary);
std::string describe_scenario(const ReplayScenarioSummary& summary);

} // namespace aethon::replay
