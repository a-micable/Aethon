#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct GainScheduleEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct GainScheduleDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class GainSchedule {
public:
    explicit GainSchedule(std::string owner = "gain_schedule");
    void insert(GainScheduleEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] GainScheduleDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<GainScheduleEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<GainScheduleEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<GainScheduleEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

GainScheduleDecision merge_gain_schedule_decisions(const std::vector<GainScheduleDecision>& decisions);
std::string render_gain_schedule_decision(const GainScheduleDecision& decision);
double gain_schedule_pressure(const GainSchedule& component);

} // namespace aethon::rf
