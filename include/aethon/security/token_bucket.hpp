#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct TokenBucketEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct TokenBucketDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class TokenBucket {
public:
    explicit TokenBucket(std::string owner = "token_bucket");
    void insert(TokenBucketEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] TokenBucketDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<TokenBucketEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<TokenBucketEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<TokenBucketEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

TokenBucketDecision merge_token_bucket_decisions(const std::vector<TokenBucketDecision>& decisions);
std::string render_token_bucket_decision(const TokenBucketDecision& decision);
double token_bucket_pressure(const TokenBucket& component);

} // namespace aethon::security
