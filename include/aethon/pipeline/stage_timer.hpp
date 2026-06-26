#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct StageTimerEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct StageTimerDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class StageTimer {
public:
    explicit StageTimer(std::string owner = "stage_timer");
    void insert(StageTimerEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] StageTimerDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<StageTimerEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<StageTimerEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<StageTimerEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

StageTimerDecision merge_stage_timer_decisions(const std::vector<StageTimerDecision>& decisions);
std::string render_stage_timer_decision(const StageTimerDecision& decision);
double stage_timer_pressure(const StageTimer& component);

} // namespace aethon::pipeline
