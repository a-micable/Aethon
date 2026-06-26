#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct OperatorIntentEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct OperatorIntentDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class OperatorIntent {
public:
    explicit OperatorIntent(std::string owner = "operator_intent");
    void insert(OperatorIntentEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] OperatorIntentDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<OperatorIntentEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<OperatorIntentEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<OperatorIntentEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

OperatorIntentDecision merge_operator_intent_decisions(const std::vector<OperatorIntentDecision>& decisions);
std::string render_operator_intent_decision(const OperatorIntentDecision& decision);
double operator_intent_pressure(const OperatorIntent& component);

} // namespace aethon::control
