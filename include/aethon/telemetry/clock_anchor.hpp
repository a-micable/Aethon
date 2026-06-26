#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct ClockAnchorEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ClockAnchorDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ClockAnchor {
public:
    explicit ClockAnchor(std::string owner = "clock_anchor");
    void insert(ClockAnchorEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ClockAnchorDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ClockAnchorEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ClockAnchorEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ClockAnchorEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ClockAnchorDecision merge_clock_anchor_decisions(const std::vector<ClockAnchorDecision>& decisions);
std::string render_clock_anchor_decision(const ClockAnchorDecision& decision);
double clock_anchor_pressure(const ClockAnchor& component);

} // namespace aethon::telemetry
