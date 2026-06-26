#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct ScanBudgetEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ScanBudgetDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ScanBudget {
public:
    explicit ScanBudget(std::string owner = "scan_budget");
    void insert(ScanBudgetEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ScanBudgetDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ScanBudgetEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ScanBudgetEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ScanBudgetEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ScanBudgetDecision merge_scan_budget_decisions(const std::vector<ScanBudgetDecision>& decisions);
std::string render_scan_budget_decision(const ScanBudgetDecision& decision);
double scan_budget_pressure(const ScanBudget& component);

} // namespace aethon::query
