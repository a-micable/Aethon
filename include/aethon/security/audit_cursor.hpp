#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::security {

struct AuditCursorEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct AuditCursorDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class AuditCursor {
public:
    explicit AuditCursor(std::string owner = "audit_cursor");
    void insert(AuditCursorEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] AuditCursorDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<AuditCursorEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<AuditCursorEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<AuditCursorEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

AuditCursorDecision merge_audit_cursor_decisions(const std::vector<AuditCursorDecision>& decisions);
std::string render_audit_cursor_decision(const AuditCursorDecision& decision);
double audit_cursor_pressure(const AuditCursor& component);

} // namespace aethon::security
