#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct SweepPlanEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SweepPlanDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SweepPlan {
public:
    explicit SweepPlan(std::string owner = "sweep_plan");
    void insert(SweepPlanEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SweepPlanDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SweepPlanEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SweepPlanEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SweepPlanEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SweepPlanDecision merge_sweep_plan_decisions(const std::vector<SweepPlanDecision>& decisions);
std::string render_sweep_plan_decision(const SweepPlanDecision& decision);
double sweep_plan_pressure(const SweepPlan& component);

} // namespace aethon::rf
