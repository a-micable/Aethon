#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct QualityFlagEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct QualityFlagDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class QualityFlag {
public:
    explicit QualityFlag(std::string owner = "quality_flag");
    void insert(QualityFlagEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] QualityFlagDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<QualityFlagEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<QualityFlagEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<QualityFlagEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

QualityFlagDecision merge_quality_flag_decisions(const std::vector<QualityFlagDecision>& decisions);
std::string render_quality_flag_decision(const QualityFlagDecision& decision);
double quality_flag_pressure(const QualityFlag& component);

} // namespace aethon::telemetry
