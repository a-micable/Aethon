#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct RunbookLinkEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RunbookLinkDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RunbookLink {
public:
    explicit RunbookLink(std::string owner = "runbook_link");
    void insert(RunbookLinkEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RunbookLinkDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RunbookLinkEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RunbookLinkEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RunbookLinkEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RunbookLinkDecision merge_runbook_link_decisions(const std::vector<RunbookLinkDecision>& decisions);
std::string render_runbook_link_decision(const RunbookLinkDecision& decision);
double runbook_link_pressure(const RunbookLink& component);

} // namespace aethon::observability
