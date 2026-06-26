#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct TelemetryBatchEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct TelemetryBatchDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class TelemetryBatch {
public:
    explicit TelemetryBatch(std::string owner = "telemetry_batch");
    void insert(TelemetryBatchEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] TelemetryBatchDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<TelemetryBatchEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<TelemetryBatchEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<TelemetryBatchEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

TelemetryBatchDecision merge_telemetry_batch_decisions(const std::vector<TelemetryBatchDecision>& decisions);
std::string render_telemetry_batch_decision(const TelemetryBatchDecision& decision);
double telemetry_batch_pressure(const TelemetryBatch& component);

} // namespace aethon::telemetry
