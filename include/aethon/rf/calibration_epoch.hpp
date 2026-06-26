#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct CalibrationEpochEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct CalibrationEpochDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class CalibrationEpoch {
public:
    explicit CalibrationEpoch(std::string owner = "calibration_epoch");
    void insert(CalibrationEpochEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] CalibrationEpochDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<CalibrationEpochEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<CalibrationEpochEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<CalibrationEpochEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

CalibrationEpochDecision merge_calibration_epoch_decisions(const std::vector<CalibrationEpochDecision>& decisions);
std::string render_calibration_epoch_decision(const CalibrationEpochDecision& decision);
double calibration_epoch_pressure(const CalibrationEpoch& component);

} // namespace aethon::rf
