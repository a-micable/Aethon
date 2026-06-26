#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct ExplainTreeEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ExplainTreeDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ExplainTree {
public:
    explicit ExplainTree(std::string owner = "explain_tree");
    void insert(ExplainTreeEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ExplainTreeDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ExplainTreeEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ExplainTreeEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ExplainTreeEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ExplainTreeDecision merge_explain_tree_decisions(const std::vector<ExplainTreeDecision>& decisions);
std::string render_explain_tree_decision(const ExplainTreeDecision& decision);
double explain_tree_pressure(const ExplainTree& component);

} // namespace aethon::query
