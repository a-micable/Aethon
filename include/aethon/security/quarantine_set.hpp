#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct QuarantineSetEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct QuarantineSetDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class QuarantineSet {
public:
    explicit QuarantineSet(std::string owner = "quarantine_set");
    void insert(QuarantineSetEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] QuarantineSetDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<QuarantineSetEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<QuarantineSetEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<QuarantineSetEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

QuarantineSetDecision merge_quarantine_set_decisions(const std::vector<QuarantineSetDecision>& decisions);
std::string render_quarantine_set_decision(const QuarantineSetDecision& decision);
double quarantine_set_pressure(const QuarantineSet& component);

} // namespace aethon::security
