#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct AckTrackerEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct AckTrackerDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class AckTracker {
public:
    explicit AckTracker(std::string owner = "ack_tracker");
    void insert(AckTrackerEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] AckTrackerDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<AckTrackerEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<AckTrackerEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<AckTrackerEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

AckTrackerDecision merge_ack_tracker_decisions(const std::vector<AckTrackerDecision>& decisions);
std::string render_ack_tracker_decision(const AckTrackerDecision& decision);
double ack_tracker_pressure(const AckTracker& component);

} // namespace aethon::control
