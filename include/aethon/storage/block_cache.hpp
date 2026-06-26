#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct BlockCacheSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct BlockCacheSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class BlockCache {
public:
    explicit BlockCache(std::string name = "block_cache");
    void observe(BlockCacheSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] BlockCacheSummary summarize() const;
    [[nodiscard]] std::optional<BlockCacheSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<BlockCacheSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<BlockCacheSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

BlockCacheSummary summarize_block_cache(const std::vector<BlockCacheSample>& samples);
double block_cache_stability_index(const BlockCacheSummary& summary);
std::string describe_block_cache(const BlockCacheSummary& summary);

} // namespace aethon::storage
