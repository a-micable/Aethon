#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct PayloadDigestEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PayloadDigestDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PayloadDigest {
public:
    explicit PayloadDigest(std::string owner = "payload_digest");
    void insert(PayloadDigestEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PayloadDigestDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PayloadDigestEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PayloadDigestEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PayloadDigestEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PayloadDigestDecision merge_payload_digest_decisions(const std::vector<PayloadDigestDecision>& decisions);
std::string render_payload_digest_decision(const PayloadDigestDecision& decision);
double payload_digest_pressure(const PayloadDigest& component);

} // namespace aethon::telemetry
