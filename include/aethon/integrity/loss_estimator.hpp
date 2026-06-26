#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::integrity {

struct LossEstimatorSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct LossEstimatorSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class LossEstimator {
public:
    explicit LossEstimator(std::string name = "loss_estimator");
    void observe(LossEstimatorSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] LossEstimatorSummary summarize() const;
    [[nodiscard]] std::optional<LossEstimatorSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<LossEstimatorSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<LossEstimatorSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

LossEstimatorSummary summarize_loss_estimator(const std::vector<LossEstimatorSample>& samples);
double loss_estimator_stability_index(const LossEstimatorSummary& summary);
std::string describe_loss_estimator(const LossEstimatorSummary& summary);

} // namespace aethon::integrity
