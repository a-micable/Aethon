#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::analysis {

struct CalibrationFitSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CalibrationFitSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CalibrationFit {
public:
    explicit CalibrationFit(std::string name = "calibration_fit");
    void observe(CalibrationFitSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CalibrationFitSummary summarize() const;
    [[nodiscard]] std::optional<CalibrationFitSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CalibrationFitSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CalibrationFitSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CalibrationFitSummary summarize_calibration_fit(const std::vector<CalibrationFitSample>& samples);
double calibration_fit_stability_index(const CalibrationFitSummary& summary);
std::string describe_calibration_fit(const CalibrationFitSummary& summary);

} // namespace aethon::analysis
