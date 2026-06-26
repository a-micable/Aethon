#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct WorkLedgerEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct WorkLedgerDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class WorkLedger {
public:
    explicit WorkLedger(std::string owner = "work_ledger");
    void insert(WorkLedgerEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] WorkLedgerDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<WorkLedgerEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<WorkLedgerEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<WorkLedgerEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

WorkLedgerDecision merge_work_ledger_decisions(const std::vector<WorkLedgerDecision>& decisions);
std::string render_work_ledger_decision(const WorkLedgerDecision& decision);
double work_ledger_pressure(const WorkLedger& component);

} // namespace aethon::pipeline
