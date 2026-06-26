#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::rf {

struct ReceiverBiasEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ReceiverBiasDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ReceiverBias {
public:
    explicit ReceiverBias(std::string owner = "receiver_bias");
    void insert(ReceiverBiasEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ReceiverBiasDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ReceiverBiasEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ReceiverBiasEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ReceiverBiasEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ReceiverBiasDecision merge_receiver_bias_decisions(const std::vector<ReceiverBiasDecision>& decisions);
std::string render_receiver_bias_decision(const ReceiverBiasDecision& decision);
double receiver_bias_pressure(const ReceiverBias& component);

} // namespace aethon::rf
