#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::plugins {

struct HookChainSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct HookChainSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class HookChain {
public:
    explicit HookChain(std::string name = "hook_chain");
    void observe(HookChainSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] HookChainSummary summarize() const;
    [[nodiscard]] std::optional<HookChainSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<HookChainSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<HookChainSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

HookChainSummary summarize_hook_chain(const std::vector<HookChainSample>& samples);
double hook_chain_stability_index(const HookChainSummary& summary);
std::string describe_hook_chain(const HookChainSummary& summary);

} // namespace aethon::plugins
