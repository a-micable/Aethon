#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct PayloadShapeEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PayloadShapeDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PayloadShape {
public:
    explicit PayloadShape(std::string owner = "payload_shape");
    void insert(PayloadShapeEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PayloadShapeDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PayloadShapeEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PayloadShapeEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PayloadShapeEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PayloadShapeDecision merge_payload_shape_decisions(const std::vector<PayloadShapeDecision>& decisions);
std::string render_payload_shape_decision(const PayloadShapeDecision& decision);
double payload_shape_pressure(const PayloadShape& component);

} // namespace aethon::telemetry
