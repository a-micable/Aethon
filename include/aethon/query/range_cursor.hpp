#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct RangeCursorEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RangeCursorDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RangeCursor {
public:
    explicit RangeCursor(std::string owner = "range_cursor");
    void insert(RangeCursorEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RangeCursorDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RangeCursorEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RangeCursorEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RangeCursorEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RangeCursorDecision merge_range_cursor_decisions(const std::vector<RangeCursorDecision>& decisions);
std::string render_range_cursor_decision(const RangeCursorDecision& decision);
double range_cursor_pressure(const RangeCursor& component);

} // namespace aethon::query
