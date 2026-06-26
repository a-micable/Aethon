#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct AlertRuleEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct AlertRuleDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class AlertRule {
public:
    explicit AlertRule(std::string owner = "alert_rule");
    void insert(AlertRuleEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] AlertRuleDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<AlertRuleEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<AlertRuleEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<AlertRuleEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

AlertRuleDecision merge_alert_rule_decisions(const std::vector<AlertRuleDecision>& decisions);
std::string render_alert_rule_decision(const AlertRuleDecision& decision);
double alert_rule_pressure(const AlertRule& component);

} // namespace aethon::observability
