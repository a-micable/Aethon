#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct RateLimitEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct RateLimitDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class RateLimit {
public:
    explicit RateLimit(std::string owner = "rate_limit");
    void insert(RateLimitEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] RateLimitDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<RateLimitEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<RateLimitEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<RateLimitEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

RateLimitDecision merge_rate_limit_decisions(const std::vector<RateLimitDecision>& decisions);
std::string render_rate_limit_decision(const RateLimitDecision& decision);
double rate_limit_pressure(const RateLimit& component);

} // namespace aethon::control
