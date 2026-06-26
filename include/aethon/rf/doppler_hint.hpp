#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct DopplerHintEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct DopplerHintDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class DopplerHint {
public:
    explicit DopplerHint(std::string owner = "doppler_hint");
    void insert(DopplerHintEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] DopplerHintDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<DopplerHintEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<DopplerHintEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<DopplerHintEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

DopplerHintDecision merge_doppler_hint_decisions(const std::vector<DopplerHintDecision>& decisions);
std::string render_doppler_hint_decision(const DopplerHintDecision& decision);
double doppler_hint_pressure(const DopplerHint& component);

} // namespace aethon::rf
