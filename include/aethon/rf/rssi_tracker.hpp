#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct RssiTrackerEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RssiTrackerDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RssiTracker {
public:
    explicit RssiTracker(std::string owner = "rssi_tracker");
    void insert(RssiTrackerEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RssiTrackerDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RssiTrackerEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RssiTrackerEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RssiTrackerEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RssiTrackerDecision merge_rssi_tracker_decisions(const std::vector<RssiTrackerDecision>& decisions);
std::string render_rssi_tracker_decision(const RssiTrackerDecision& decision);
double rssi_tracker_pressure(const RssiTracker& component);

} // namespace aethon::rf
