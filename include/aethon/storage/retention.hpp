#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct RetentionPolicySample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct RetentionPolicySummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class RetentionPolicy {
public:
    explicit RetentionPolicy(std::string name = "retention");
    void observe(RetentionPolicySample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] RetentionPolicySummary summarize() const;
    [[nodiscard]] std::optional<RetentionPolicySample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<RetentionPolicySample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<RetentionPolicySample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

RetentionPolicySummary summarize_retention(const std::vector<RetentionPolicySample>& samples);
double retention_stability_index(const RetentionPolicySummary& summary);
std::string describe_retention(const RetentionPolicySummary& summary);

} // namespace aethon::storage
