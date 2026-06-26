#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct LaneAllocatorEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct LaneAllocatorDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class LaneAllocator {
public:
    explicit LaneAllocator(std::string owner = "lane_allocator");
    void insert(LaneAllocatorEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] LaneAllocatorDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<LaneAllocatorEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<LaneAllocatorEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<LaneAllocatorEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

LaneAllocatorDecision merge_lane_allocator_decisions(const std::vector<LaneAllocatorDecision>& decisions);
std::string render_lane_allocator_decision(const LaneAllocatorDecision& decision);
double lane_allocator_pressure(const LaneAllocator& component);

} // namespace aethon::pipeline
