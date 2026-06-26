#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::protocol {

struct CapabilitySetSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CapabilitySetSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CapabilitySet {
public:
    explicit CapabilitySet(std::string name = "capability_set");
    void observe(CapabilitySetSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CapabilitySetSummary summarize() const;
    [[nodiscard]] std::optional<CapabilitySetSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CapabilitySetSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CapabilitySetSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CapabilitySetSummary summarize_capability_set(const std::vector<CapabilitySetSample>& samples);
double capability_set_stability_index(const CapabilitySetSummary& summary);
std::string describe_capability_set(const CapabilitySetSummary& summary);

} // namespace aethon::protocol
