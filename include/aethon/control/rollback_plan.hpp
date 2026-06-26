#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct RollbackPlanEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RollbackPlanDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RollbackPlan {
public:
    explicit RollbackPlan(std::string owner = "rollback_plan");
    void insert(RollbackPlanEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RollbackPlanDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RollbackPlanEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RollbackPlanEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RollbackPlanEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RollbackPlanDecision merge_rollback_plan_decisions(const std::vector<RollbackPlanDecision>& decisions);
std::string render_rollback_plan_decision(const RollbackPlanDecision& decision);
double rollback_plan_pressure(const RollbackPlan& component);

} // namespace aethon::control
