#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct KeyEpochEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct KeyEpochDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class KeyEpoch {
public:
    explicit KeyEpoch(std::string owner = "key_epoch");
    void insert(KeyEpochEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] KeyEpochDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<KeyEpochEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<KeyEpochEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<KeyEpochEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

KeyEpochDecision merge_key_epoch_decisions(const std::vector<KeyEpochDecision>& decisions);
std::string render_key_epoch_decision(const KeyEpochDecision& decision);
double key_epoch_pressure(const KeyEpoch& component);

} // namespace aethon::security
