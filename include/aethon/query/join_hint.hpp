#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct JoinHintEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct JoinHintDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class JoinHint {
public:
    explicit JoinHint(std::string owner = "join_hint");
    void insert(JoinHintEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] JoinHintDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<JoinHintEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<JoinHintEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<JoinHintEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

JoinHintDecision merge_join_hint_decisions(const std::vector<JoinHintDecision>& decisions);
std::string render_join_hint_decision(const JoinHintDecision& decision);
double join_hint_pressure(const JoinHint& component);

} // namespace aethon::query
