#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::telemetry {

struct MessageCatalogEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct MessageCatalogDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class MessageCatalog {
public:
    explicit MessageCatalog(std::string owner = "message_catalog");
    void insert(MessageCatalogEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] MessageCatalogDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<MessageCatalogEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<MessageCatalogEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<MessageCatalogEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

MessageCatalogDecision merge_message_catalog_decisions(const std::vector<MessageCatalogDecision>& decisions);
std::string render_message_catalog_decision(const MessageCatalogDecision& decision);
double message_catalog_pressure(const MessageCatalog& component);

} // namespace aethon::telemetry
