#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::integrity {

struct DigestCacheSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct DigestCacheSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class DigestCache {
public:
    explicit DigestCache(std::string name = "digest_cache");
    void observe(DigestCacheSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] DigestCacheSummary summarize() const;
    [[nodiscard]] std::optional<DigestCacheSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<DigestCacheSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<DigestCacheSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

DigestCacheSummary summarize_digest_cache(const std::vector<DigestCacheSample>& samples);
double digest_cache_stability_index(const DigestCacheSummary& summary);
std::string describe_digest_cache(const DigestCacheSummary& summary);

} // namespace aethon::integrity
