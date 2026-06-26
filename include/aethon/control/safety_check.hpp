#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct SafetyCheckEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct SafetyCheckDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class SafetyCheck {
public:
    explicit SafetyCheck(std::string owner = "safety_check");
    void insert(SafetyCheckEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] SafetyCheckDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<SafetyCheckEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<SafetyCheckEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<SafetyCheckEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

SafetyCheckDecision merge_safety_check_decisions(const std::vector<SafetyCheckDecision>& decisions);
std::string render_safety_check_decision(const SafetyCheckDecision& decision);
double safety_check_pressure(const SafetyCheck& component);

} // namespace aethon::control
