#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct TimePartitionEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct TimePartitionDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class TimePartition {
public:
    explicit TimePartition(std::string owner = "time_partition");
    void insert(TimePartitionEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] TimePartitionDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<TimePartitionEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<TimePartitionEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<TimePartitionEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

TimePartitionDecision merge_time_partition_decisions(const std::vector<TimePartitionDecision>& decisions);
std::string render_time_partition_decision(const TimePartitionDecision& decision);
double time_partition_pressure(const TimePartition& component);

} // namespace aethon::query
