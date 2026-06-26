#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct PhaseModelEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PhaseModelDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PhaseModel {
public:
    explicit PhaseModel(std::string owner = "phase_model");
    void insert(PhaseModelEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PhaseModelDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PhaseModelEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PhaseModelEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PhaseModelEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PhaseModelDecision merge_phase_model_decisions(const std::vector<PhaseModelDecision>& decisions);
std::string render_phase_model_decision(const PhaseModelDecision& decision);
double phase_model_pressure(const PhaseModel& component);

} // namespace aethon::rf
