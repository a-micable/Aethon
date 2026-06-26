#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct AntennaProfileEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct AntennaProfileDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class AntennaProfile {
public:
    explicit AntennaProfile(std::string owner = "antenna_profile");
    void insert(AntennaProfileEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] AntennaProfileDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<AntennaProfileEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<AntennaProfileEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<AntennaProfileEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

AntennaProfileDecision merge_antenna_profile_decisions(const std::vector<AntennaProfileDecision>& decisions);
std::string render_antenna_profile_decision(const AntennaProfileDecision& decision);
double antenna_profile_pressure(const AntennaProfile& component);

} // namespace aethon::rf
