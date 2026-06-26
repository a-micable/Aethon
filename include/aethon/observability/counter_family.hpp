#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct CounterFamilyEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct CounterFamilyDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class CounterFamily {
public:
    explicit CounterFamily(std::string owner = "counter_family");
    void insert(CounterFamilyEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] CounterFamilyDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<CounterFamilyEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<CounterFamilyEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<CounterFamilyEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

CounterFamilyDecision merge_counter_family_decisions(const std::vector<CounterFamilyDecision>& decisions);
std::string render_counter_family_decision(const CounterFamilyDecision& decision);
double counter_family_pressure(const CounterFamily& component);

} // namespace aethon::observability
