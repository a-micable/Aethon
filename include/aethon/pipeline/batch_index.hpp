#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::pipeline {

struct BatchIndexEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct BatchIndexDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class BatchIndex {
public:
    explicit BatchIndex(std::string owner = "batch_index");
    void insert(BatchIndexEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] BatchIndexDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<BatchIndexEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<BatchIndexEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<BatchIndexEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

BatchIndexDecision merge_batch_index_decisions(const std::vector<BatchIndexDecision>& decisions);
std::string render_batch_index_decision(const BatchIndexDecision& decision);
double batch_index_pressure(const BatchIndex& component);

} // namespace aethon::pipeline
