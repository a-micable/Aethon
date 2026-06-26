#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct ControlJournalEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ControlJournalDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ControlJournal {
public:
    explicit ControlJournal(std::string owner = "control_journal");
    void insert(ControlJournalEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ControlJournalDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ControlJournalEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ControlJournalEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ControlJournalEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ControlJournalDecision merge_control_journal_decisions(const std::vector<ControlJournalDecision>& decisions);
std::string render_control_journal_decision(const ControlJournalDecision& decision);
double control_journal_pressure(const ControlJournal& component);

} // namespace aethon::control
