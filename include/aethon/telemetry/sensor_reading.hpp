#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct SensorReadingEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SensorReadingDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SensorReading {
public:
    explicit SensorReading(std::string owner = "sensor_reading");
    void insert(SensorReadingEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SensorReadingDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SensorReadingEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SensorReadingEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SensorReadingEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SensorReadingDecision merge_sensor_reading_decisions(const std::vector<SensorReadingDecision>& decisions);
std::string render_sensor_reading_decision(const SensorReadingDecision& decision);
double sensor_reading_pressure(const SensorReading& component);

} // namespace aethon::telemetry
