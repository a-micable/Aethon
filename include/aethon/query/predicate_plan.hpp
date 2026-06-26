#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct PredicatePlanEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PredicatePlanDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PredicatePlan {
public:
    explicit PredicatePlan(std::string owner = "predicate_plan");
    void insert(PredicatePlanEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PredicatePlanDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PredicatePlanEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PredicatePlanEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PredicatePlanEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PredicatePlanDecision merge_predicate_plan_decisions(const std::vector<PredicatePlanDecision>& decisions);
std::string render_predicate_plan_decision(const PredicatePlanDecision& decision);
double predicate_plan_pressure(const PredicatePlan& component);

} // namespace aethon::query
