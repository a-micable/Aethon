#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct BurstClassifierSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct BurstClassifierSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class BurstClassifier {
public:
    explicit BurstClassifier(std::string name = "burst_classifier");
    void observe(BurstClassifierSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] BurstClassifierSummary summarize() const;
    [[nodiscard]] std::optional<BurstClassifierSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<BurstClassifierSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<BurstClassifierSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

BurstClassifierSummary summarize_burst_classifier(const std::vector<BurstClassifierSample>& samples);
double burst_classifier_stability_index(const BurstClassifierSummary& summary);
std::string describe_burst_classifier(const BurstClassifierSummary& summary);

} // namespace aethon::analysis
