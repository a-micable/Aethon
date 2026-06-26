#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct AnomalyScoreSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct AnomalyScoreSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class AnomalyScore {
public:
    explicit AnomalyScore(std::string name = "anomaly_score");
    void observe(AnomalyScoreSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] AnomalyScoreSummary summarize() const;
    [[nodiscard]] std::optional<AnomalyScoreSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<AnomalyScoreSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<AnomalyScoreSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

AnomalyScoreSummary summarize_anomaly_score(const std::vector<AnomalyScoreSample>& samples);
double anomaly_score_stability_index(const AnomalyScoreSummary& summary);
std::string describe_anomaly_score(const AnomalyScoreSummary& summary);

} // namespace aethon::analysis
