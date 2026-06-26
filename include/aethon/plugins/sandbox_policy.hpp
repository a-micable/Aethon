#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::plugins {

struct SandboxPolicySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct SandboxPolicySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class SandboxPolicy {
public:
    explicit SandboxPolicy(std::string name = "sandbox_policy");
    void observe(SandboxPolicySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] SandboxPolicySummary summarize() const;
    [[nodiscard]] std::optional<SandboxPolicySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<SandboxPolicySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<SandboxPolicySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

SandboxPolicySummary summarize_sandbox_policy(const std::vector<SandboxPolicySample>& samples);
double sandbox_policy_stability_index(const SandboxPolicySummary& summary);
std::string describe_sandbox_policy(const SandboxPolicySummary& summary);

} // namespace aethon::plugins
