#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct FieldProjectionEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct FieldProjectionDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class FieldProjection {
public:
    explicit FieldProjection(std::string owner = "field_projection");
    void insert(FieldProjectionEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] FieldProjectionDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<FieldProjectionEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<FieldProjectionEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<FieldProjectionEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

FieldProjectionDecision merge_field_projection_decisions(const std::vector<FieldProjectionDecision>& decisions);
std::string render_field_projection_decision(const FieldProjectionDecision& decision);
double field_projection_pressure(const FieldProjection& component);

} // namespace aethon::query
