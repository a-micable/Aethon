#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::diagnostics {

struct HealthReportSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct HealthReportSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class HealthReport {
public:
    explicit HealthReport(std::string name = "health_report");
    void observe(HealthReportSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] HealthReportSummary summarize() const;
    [[nodiscard]] std::optional<HealthReportSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<HealthReportSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<HealthReportSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

HealthReportSummary summarize_health_report(const std::vector<HealthReportSample>& samples);
double health_report_stability_index(const HealthReportSummary& summary);
std::string describe_health_report(const HealthReportSummary& summary);

} // namespace aethon::diagnostics
