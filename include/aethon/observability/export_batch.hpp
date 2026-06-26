#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::observability {

struct ExportBatchEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct ExportBatchDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class ExportBatch {
public:
    explicit ExportBatch(std::string owner = "export_batch");
    void insert(ExportBatchEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] ExportBatchDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<ExportBatchEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<ExportBatchEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<ExportBatchEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

ExportBatchDecision merge_export_batch_decisions(const std::vector<ExportBatchDecision>& decisions);
std::string render_export_batch_decision(const ExportBatchDecision& decision);
double export_batch_pressure(const ExportBatch& component);

} // namespace aethon::observability
