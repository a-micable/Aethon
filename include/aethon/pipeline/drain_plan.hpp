#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct DrainPlanEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct DrainPlanDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class DrainPlan {
public:
    explicit DrainPlan(std::string owner = "drain_plan");
    void insert(DrainPlanEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] DrainPlanDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<DrainPlanEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<DrainPlanEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<DrainPlanEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

DrainPlanDecision merge_drain_plan_decisions(const std::vector<DrainPlanDecision>& decisions);
std::string render_drain_plan_decision(const DrainPlanDecision& decision);
double drain_plan_pressure(const DrainPlan& component);

} // namespace aethon::pipeline
