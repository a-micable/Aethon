#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::plugins {

struct ModuleStateSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct ModuleStateSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class ModuleState {
public:
    explicit ModuleState(std::string name = "module_state");
    void observe(ModuleStateSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] ModuleStateSummary summarize() const;
    [[nodiscard]] std::optional<ModuleStateSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<ModuleStateSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<ModuleStateSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

ModuleStateSummary summarize_module_state(const std::vector<ModuleStateSample>& samples);
double module_state_stability_index(const ModuleStateSummary& summary);
std::string describe_module_state(const ModuleStateSummary& summary);

} // namespace aethon::plugins
