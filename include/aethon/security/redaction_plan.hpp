#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct RedactionPlanEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RedactionPlanDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RedactionPlan {
public:
    explicit RedactionPlan(std::string owner = "redaction_plan");
    void insert(RedactionPlanEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RedactionPlanDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RedactionPlanEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RedactionPlanEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RedactionPlanEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RedactionPlanDecision merge_redaction_plan_decisions(const std::vector<RedactionPlanDecision>& decisions);
std::string render_redaction_plan_decision(const RedactionPlanDecision& decision);
double redaction_plan_pressure(const RedactionPlan& component);

} // namespace aethon::security
