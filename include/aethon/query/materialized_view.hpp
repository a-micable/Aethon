#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::query {

struct MaterializedViewEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct MaterializedViewDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class MaterializedView {
public:
    explicit MaterializedView(std::string owner = "materialized_view");
    void insert(MaterializedViewEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] MaterializedViewDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<MaterializedViewEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<MaterializedViewEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<MaterializedViewEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

MaterializedViewDecision merge_materialized_view_decisions(const std::vector<MaterializedViewDecision>& decisions);
std::string render_materialized_view_decision(const MaterializedViewDecision& decision);
double materialized_view_pressure(const MaterializedView& component);

} // namespace aethon::query
