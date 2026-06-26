#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct FilterCacheEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct FilterCacheDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class FilterCache {
public:
    explicit FilterCache(std::string owner = "filter_cache");
    void insert(FilterCacheEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] FilterCacheDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<FilterCacheEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<FilterCacheEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<FilterCacheEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

FilterCacheDecision merge_filter_cache_decisions(const std::vector<FilterCacheDecision>& decisions);
std::string render_filter_cache_decision(const FilterCacheDecision& decision);
double filter_cache_pressure(const FilterCache& component);

} // namespace aethon::query
