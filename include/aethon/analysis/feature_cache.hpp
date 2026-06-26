#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct FeatureCacheSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct FeatureCacheSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class FeatureCache {
public:
    explicit FeatureCache(std::string name = "feature_cache");
    void observe(FeatureCacheSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] FeatureCacheSummary summarize() const;
    [[nodiscard]] std::optional<FeatureCacheSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<FeatureCacheSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<FeatureCacheSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

FeatureCacheSummary summarize_feature_cache(const std::vector<FeatureCacheSample>& samples);
double feature_cache_stability_index(const FeatureCacheSummary& summary);
std::string describe_feature_cache(const FeatureCacheSummary& summary);

} // namespace aethon::analysis
