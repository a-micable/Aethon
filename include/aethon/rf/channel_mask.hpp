#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct ChannelMaskEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ChannelMaskDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ChannelMask {
public:
    explicit ChannelMask(std::string owner = "channel_mask");
    void insert(ChannelMaskEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ChannelMaskDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ChannelMaskEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ChannelMaskEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ChannelMaskEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ChannelMaskDecision merge_channel_mask_decisions(const std::vector<ChannelMaskDecision>& decisions);
std::string render_channel_mask_decision(const ChannelMaskDecision& decision);
double channel_mask_pressure(const ChannelMask& component);

} // namespace aethon::rf
