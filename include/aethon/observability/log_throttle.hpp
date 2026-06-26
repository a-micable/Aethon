#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct LogThrottleEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct LogThrottleDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class LogThrottle {
public:
    explicit LogThrottle(std::string owner = "log_throttle");
    void insert(LogThrottleEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] LogThrottleDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<LogThrottleEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<LogThrottleEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<LogThrottleEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

LogThrottleDecision merge_log_throttle_decisions(const std::vector<LogThrottleDecision>& decisions);
std::string render_log_throttle_decision(const LogThrottleDecision& decision);
double log_throttle_pressure(const LogThrottle& component);

} // namespace aethon::observability
