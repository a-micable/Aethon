#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::sensors {

struct CalibrationStoreSample {
    std::uint64_t timestamp_ns = 0;
    double value = 0.0;
    double weight = 1.0;
    std::string label;
};

struct CalibrationStoreSummary {
    std::size_t count = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double average = 0.0;
    double confidence = 0.0;
    std::string note;
};

class CalibrationStore {
public:
    explicit CalibrationStore(std::string name = "calibration_store");
    void observe(CalibrationStoreSample sample);
    void clear_before(std::uint64_t timestamp_ns);
    [[nodiscard]] CalibrationStoreSummary summarize() const;
    [[nodiscard]] std::optional<CalibrationStoreSample> latest(std::string_view label) const;
    [[nodiscard]] std::vector<CalibrationStoreSample> select(double threshold) const;
    [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
private:
    std::string name_;
    std::deque<CalibrationStoreSample> samples_;
    std::map<std::string, std::uint64_t, std::less<>> label_counts_;
};

CalibrationStoreSummary summarize_calibration_store(const std::vector<CalibrationStoreSample>& samples);
double calibration_store_stability_index(const CalibrationStoreSummary& summary);
std::string describe_calibration_store(const CalibrationStoreSummary& summary);

} // namespace aethon::sensors
