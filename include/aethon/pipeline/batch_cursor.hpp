#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct BatchCursorEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct BatchCursorDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class BatchCursor {
public:
    explicit BatchCursor(std::string owner = "batch_cursor");
    void insert(BatchCursorEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] BatchCursorDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<BatchCursorEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<BatchCursorEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<BatchCursorEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

BatchCursorDecision merge_batch_cursor_decisions(const std::vector<BatchCursorDecision>& decisions);
std::string render_batch_cursor_decision(const BatchCursorDecision& decision);
double batch_cursor_pressure(const BatchCursor& component);

} // namespace aethon::pipeline
