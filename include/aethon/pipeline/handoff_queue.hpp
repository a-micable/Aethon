#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct HandoffQueueEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct HandoffQueueDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class HandoffQueue {
public:
    explicit HandoffQueue(std::string owner = "handoff_queue");
    void insert(HandoffQueueEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] HandoffQueueDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<HandoffQueueEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<HandoffQueueEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<HandoffQueueEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

HandoffQueueDecision merge_handoff_queue_decisions(const std::vector<HandoffQueueDecision>& decisions);
std::string render_handoff_queue_decision(const HandoffQueueDecision& decision);
double handoff_queue_pressure(const HandoffQueue& component);

} // namespace aethon::pipeline
