#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::control {

struct CommandQueueEntry {
    std::uint64_t tick = 0;
    std::uint32_t ordinal = 0;
    double score = 0.0;
    std::string key;
    std::string detail;
};

struct CommandQueueDecision {
    bool accepted = false;
    double score = 0.0;
    std::string reason;
    std::vector<std::string> labels;
};

class CommandQueue {
public:
    explicit CommandQueue(std::string owner = "command_queue");
    void insert(CommandQueueEntry entry);
    void remove_before(std::uint64_t tick);
    [[nodiscard]] CommandQueueDecision evaluate(std::string_view key, double threshold) const;
    [[nodiscard]] std::vector<CommandQueueEntry> drain(double minimum_score) const;
    [[nodiscard]] std::optional<CommandQueueEntry> find(std::string_view key) const;
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const std::string& owner() const noexcept { return owner_; }
private:
    std::string owner_;
    std::deque<CommandQueueEntry> entries_;
    std::map<std::string, std::uint32_t, std::less<>> key_counts_;
};

CommandQueueDecision merge_command_queue_decisions(const std::vector<CommandQueueDecision>& decisions);
std::string render_command_queue_decision(const CommandQueueDecision& decision);
double command_queue_pressure(const CommandQueue& component);

} // namespace aethon::control
