#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct PolicyDigestEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PolicyDigestDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PolicyDigest {
public:
    explicit PolicyDigest(std::string owner = "policy_digest");
    void insert(PolicyDigestEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PolicyDigestDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PolicyDigestEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PolicyDigestEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PolicyDigestEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PolicyDigestDecision merge_policy_digest_decisions(const std::vector<PolicyDigestDecision>& decisions);
std::string render_policy_digest_decision(const PolicyDigestDecision& decision);
double policy_digest_pressure(const PolicyDigest& component);

} // namespace aethon::security
