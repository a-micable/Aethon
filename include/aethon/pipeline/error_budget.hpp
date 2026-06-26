#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct ErrorBudgetEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ErrorBudgetDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ErrorBudget {
public:
    explicit ErrorBudget(std::string owner = "error_budget");
    void insert(ErrorBudgetEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ErrorBudgetDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ErrorBudgetEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ErrorBudgetEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ErrorBudgetEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ErrorBudgetDecision merge_error_budget_decisions(const std::vector<ErrorBudgetDecision>& decisions);
std::string render_error_budget_decision(const ErrorBudgetDecision& decision);
double error_budget_pressure(const ErrorBudget& component);

} // namespace aethon::pipeline
