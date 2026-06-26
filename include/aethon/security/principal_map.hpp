#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct PrincipalMapEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct PrincipalMapDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class PrincipalMap {
public:
    explicit PrincipalMap(std::string owner = "principal_map");
    void insert(PrincipalMapEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] PrincipalMapDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<PrincipalMapEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<PrincipalMapEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<PrincipalMapEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

PrincipalMapDecision merge_principal_map_decisions(const std::vector<PrincipalMapDecision>& decisions);
std::string render_principal_map_decision(const PrincipalMapDecision& decision);
double principal_map_pressure(const PrincipalMap& component);

} // namespace aethon::security
