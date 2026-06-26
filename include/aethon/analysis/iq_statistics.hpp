#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct IqStatisticsSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct IqStatisticsSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class IqStatistics {
public:
    explicit IqStatistics(std::string name = "iq_statistics");
    void observe(IqStatisticsSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] IqStatisticsSummary summarize() const;
    [[nodiscard]] std::optional<IqStatisticsSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<IqStatisticsSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<IqStatisticsSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

IqStatisticsSummary summarize_iq_statistics(const std::vector<IqStatisticsSample>& samples);
double iq_statistics_stability_index(const IqStatisticsSummary& summary);
std::string describe_iq_statistics(const IqStatisticsSummary& summary);

} // namespace aethon::analysis
