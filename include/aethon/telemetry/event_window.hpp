#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct EventWindowEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct EventWindowDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class EventWindow {
public:
    explicit EventWindow(std::string owner = "event_window");
    void insert(EventWindowEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] EventWindowDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<EventWindowEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<EventWindowEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<EventWindowEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

EventWindowDecision merge_event_window_decisions(const std::vector<EventWindowDecision>& decisions);
std::string render_event_window_decision(const EventWindowDecision& decision);
double event_window_pressure(const EventWindow& component);

} // namespace aethon::telemetry
