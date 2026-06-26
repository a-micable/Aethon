#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::integrity {

struct TamperReportSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct TamperReportSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class TamperReport {
public:
    explicit TamperReport(std::string name = "tamper_report");
    void observe(TamperReportSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] TamperReportSummary summarize() const;
    [[nodiscard]] std::optional<TamperReportSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<TamperReportSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<TamperReportSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

TamperReportSummary summarize_tamper_report(const std::vector<TamperReportSample>& samples);
double tamper_report_stability_index(const TamperReportSummary& summary);
std::string describe_tamper_report(const TamperReportSummary& summary);

} // namespace aethon::integrity
