#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct TrustScoreEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct TrustScoreDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class TrustScore {
public:
    explicit TrustScore(std::string owner = "trust_score");
    void insert(TrustScoreEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] TrustScoreDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<TrustScoreEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<TrustScoreEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<TrustScoreEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

TrustScoreDecision merge_trust_score_decisions(const std::vector<TrustScoreDecision>& decisions);
std::string render_trust_score_decision(const TrustScoreDecision& decision);
double trust_score_pressure(const TrustScore& component);

} // namespace aethon::security
