#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct FlowGateEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct FlowGateDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class FlowGate {
public:
    explicit FlowGate(std::string owner = "flow_gate");
    void insert(FlowGateEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] FlowGateDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<FlowGateEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<FlowGateEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<FlowGateEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

FlowGateDecision merge_flow_gate_decisions(const std::vector<FlowGateDecision>& decisions);
std::string render_flow_gate_decision(const FlowGateDecision& decision);
double flow_gate_pressure(const FlowGate& component);

} // namespace aethon::pipeline
