#pragma once

#include "aethon/common/bytes.hpp"
#include "aethon/protocol/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::control {

using CommandId = std::uint64_t;

enum class CommandKind : std::uint8_t {
    configure,
    start_stream,
    stop_stream,
    calibrate,
    reset,
    custom,
};

enum class CommandState : std::uint8_t {
    pending,
    sent,
    acknowledged,
    failed,
    expired,
    cancelled,
};

enum class AckStatus : std::uint8_t {
    accepted,
    rejected,
    busy,
    malformed,
    unknown_command,
};

struct RetryPolicy {
    std::uint32_t max_attempts = 3;
    std::uint64_t initial_delay_ns = 1'000'000;
    std::uint64_t max_delay_ns = 60'000'000'000;
    double multiplier = 2.0;
};

struct ControlCommand {
    CommandId id = 0;
    CommandKind kind = CommandKind::custom;
    protocol::DeviceId device = 0;
    std::uint64_t created_time_ns = 0;
    std::uint64_t expires_time_ns = 0;
    Bytes payload;
    std::string label;
};

struct CommandAck {
    CommandId command_id = 0;
    protocol::DeviceId device = 0;
    AckStatus status = AckStatus::accepted;
    std::uint64_t time_ns = 0;
    std::string message;
};

struct JournalEntry {
    ControlCommand command;
    CommandState state = CommandState::pending;
    std::uint32_t attempts = 0;
    std::uint64_t last_send_time_ns = 0;
    std::uint64_t next_attempt_time_ns = 0;
    std::optional<CommandAck> ack;
    std::vector<std::string> notes;
};

struct JournalAppendResult {
    bool appended = false;
    CommandId id = 0;
    std::string reason;
};

struct JournalDispatch {
    CommandId id = 0;
    protocol::Packet packet;
    std::uint32_t attempt = 0;
    std::uint64_t next_attempt_time_ns = 0;
};

struct JournalAckResult {
    bool matched = false;
    CommandState state = CommandState::pending;
    std::string reason;
};

struct CommandJournalStats {
    std::uint64_t appended = 0;
    std::uint64_t dispatched = 0;
    std::uint64_t acknowledged = 0;
    std::uint64_t failed = 0;
    std::uint64_t expired = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t duplicate_rejections = 0;
};

struct CommandJournalConfig {
    std::size_t max_entries = 4096;
    std::uint64_t default_ttl_ns = 30'000'000'000;
    protocol::DeviceId controller_device = 0;
    RetryPolicy retry;
};

class CommandJournal {
public:
    explicit CommandJournal(CommandJournalConfig config = {});

    [[nodiscard]] const CommandJournalConfig& config() const noexcept;
    [[nodiscard]] CommandJournalStats stats() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::optional<JournalEntry> find(CommandId id) const;
    [[nodiscard]] std::vector<JournalEntry> entries() const;
    [[nodiscard]] std::vector<JournalEntry> entries_for_device(protocol::DeviceId device) const;

    [[nodiscard]] JournalAppendResult append(ControlCommand command);
    [[nodiscard]] std::vector<JournalDispatch> due(std::uint64_t now_ns, std::size_t max_count);
    [[nodiscard]] JournalAckResult acknowledge(CommandAck ack);
    [[nodiscard]] bool cancel(CommandId id, std::string reason);
    [[nodiscard]] std::vector<JournalEntry> expire(std::uint64_t now_ns);
    void compact_completed(std::size_t keep_recent);
    void clear();

private:
    [[nodiscard]] CommandId next_id();
    [[nodiscard]] bool duplicate_active_command(const ControlCommand& command) const;
    [[nodiscard]] protocol::Packet make_packet(const JournalEntry& entry) const;
    [[nodiscard]] std::uint64_t retry_delay(std::uint32_t attempts) const;
    [[nodiscard]] bool terminal(CommandState state) const noexcept;
    JournalEntry& require_entry(CommandId id);
    void note(JournalEntry& entry, std::string message);

    CommandJournalConfig config_;
    std::map<CommandId, JournalEntry> entries_;
    CommandJournalStats stats_;
    CommandId next_id_ = 1;
};

[[nodiscard]] ControlCommand make_configure_command(protocol::DeviceId device,
                                                    std::uint64_t now_ns,
                                                    Bytes payload,
                                                    std::string label = {});
[[nodiscard]] ControlCommand make_start_stream_command(protocol::DeviceId device,
                                                       std::uint64_t now_ns,
                                                       Bytes payload = {});
[[nodiscard]] ControlCommand make_stop_stream_command(protocol::DeviceId device,
                                                      std::uint64_t now_ns,
                                                      Bytes payload = {});
[[nodiscard]] std::string command_kind_name(CommandKind kind);
[[nodiscard]] std::string command_state_name(CommandState state);
[[nodiscard]] std::string ack_status_name(AckStatus status);
[[nodiscard]] std::string render_journal_entry(const JournalEntry& entry);
[[nodiscard]] std::string render_command_journal_stats(const CommandJournalStats& stats);

} // namespace aethon::control
