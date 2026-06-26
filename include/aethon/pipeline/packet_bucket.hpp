#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct PacketBucketEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PacketBucketDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PacketBucket {
public:
    explicit PacketBucket(std::string owner = "packet_bucket");
    void insert(PacketBucketEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PacketBucketDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PacketBucketEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PacketBucketEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PacketBucketEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PacketBucketDecision merge_packet_bucket_decisions(const std::vector<PacketBucketDecision>& decisions);
std::string render_packet_bucket_decision(const PacketBucketDecision& decision);
double packet_bucket_pressure(const PacketBucket& component);

} // namespace aethon::pipeline
