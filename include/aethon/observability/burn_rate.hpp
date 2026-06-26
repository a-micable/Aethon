#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct BurnRateEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct BurnRateDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class BurnRate {
public:
    explicit BurnRate(std::string owner = "burn_rate");
    void insert(BurnRateEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] BurnRateDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<BurnRateEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<BurnRateEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<BurnRateEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

BurnRateDecision merge_burn_rate_decisions(const std::vector<BurnRateDecision>& decisions);
std::string render_burn_rate_decision(const BurnRateDecision& decision);
double burn_rate_pressure(const BurnRate& component);

} // namespace aethon::observability
