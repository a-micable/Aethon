#include "aethon/control/command_journal.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace aethon::control {
namespace {

Bytes encode_command_payload(CommandId id, CommandKind kind, const Bytes& body) {
    Bytes payload;
    payload.reserve(9 + body.size());
    for (int shift = 56; shift >= 0; shift -= 8) {
        payload.push_back(static_cast<std::uint8_t>((id >> shift) & 0xffu));
    }
    payload.push_back(static_cast<std::uint8_t>(kind));
    payload.insert(payload.end(), body.begin(), body.end());
    return payload;
}

bool same_active_intent(const ControlCommand& lhs, const ControlCommand& rhs) {
    return lhs.device == rhs.device
        && lhs.kind == rhs.kind
        && lhs.label == rhs.label
        && lhs.payload == rhs.payload;
}

std::uint64_t saturating_multiply(std::uint64_t value, double multiplier, std::uint64_t limit) {
    if (value >= limit) {
        return limit;
    }
    const auto scaled = static_cast<long double>(value) * static_cast<long double>(multiplier);
    if (scaled >= static_cast<long double>(limit)) {
        return limit;
    }
    return std::max<std::uint64_t>(1, static_cast<std::uint64_t>(scaled));
}

} // namespace

CommandJournal::CommandJournal(CommandJournalConfig config)
    : config_(config) {}

const CommandJournalConfig& CommandJournal::config() const noexcept {
    return config_;
}

CommandJournalStats CommandJournal::stats() const noexcept {
    return stats_;
}

std::size_t CommandJournal::size() const noexcept {
    return entries_.size();
}

bool CommandJournal::empty() const noexcept {
    return entries_.empty();
}

std::optional<JournalEntry> CommandJournal::find(CommandId id) const {
    auto found = entries_.find(id);
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::vector<JournalEntry> CommandJournal::entries() const {
    std::vector<JournalEntry> result;
    result.reserve(entries_.size());
    for (const auto& [unused, entry] : entries_) {
        (void)unused;
        result.push_back(entry);
    }
    return result;
}

std::vector<JournalEntry> CommandJournal::entries_for_device(protocol::DeviceId device) const {
    std::vector<JournalEntry> result;
    for (const auto& [unused, entry] : entries_) {
        (void)unused;
        if (entry.command.device == device) {
            result.push_back(entry);
        }
    }
    return result;
}

JournalAppendResult CommandJournal::append(ControlCommand command) {
    if (entries_.size() >= config_.max_entries) {
        return JournalAppendResult{false, 0, "command journal capacity reached"};
    }
    if (duplicate_active_command(command)) {
        ++stats_.duplicate_rejections;
        return JournalAppendResult{false, 0, "duplicate active command"};
    }
    if (command.id == 0) {
        command.id = next_id();
    } else {
        next_id_ = std::max(next_id_, command.id + 1);
    }
    if (command.expires_time_ns == 0) {
        command.expires_time_ns = command.created_time_ns + config_.default_ttl_ns;
    }

    JournalEntry entry;
    entry.command = std::move(command);
    entry.state = CommandState::pending;
    entry.next_attempt_time_ns = entry.command.created_time_ns;
    note(entry, "command appended");

    const auto id = entry.command.id;
    entries_[id] = std::move(entry);
    ++stats_.appended;
    return JournalAppendResult{true, id, "appended"};
}

std::vector<JournalDispatch> CommandJournal::due(std::uint64_t now_ns, std::size_t max_count) {
    std::vector<JournalDispatch> dispatches;
    for (auto& [id, entry] : entries_) {
        if (dispatches.size() >= max_count) {
            break;
        }
        if (terminal(entry.state)) {
            continue;
        }
        if (entry.command.expires_time_ns != 0 && now_ns >= entry.command.expires_time_ns) {
            entry.state = CommandState::expired;
            note(entry, "command expired before dispatch");
            ++stats_.expired;
            continue;
        }
        if (entry.next_attempt_time_ns > now_ns) {
            continue;
        }
        if (entry.attempts >= config_.retry.max_attempts) {
            entry.state = CommandState::failed;
            note(entry, "retry attempts exhausted");
            ++stats_.failed;
            continue;
        }

        ++entry.attempts;
        entry.state = CommandState::sent;
        entry.last_send_time_ns = now_ns;
        entry.next_attempt_time_ns = now_ns + retry_delay(entry.attempts);
        note(entry, "command dispatched");

        dispatches.push_back(JournalDispatch{
            id,
            make_packet(entry),
            entry.attempts,
            entry.next_attempt_time_ns,
        });
        ++stats_.dispatched;
    }
    return dispatches;
}

JournalAckResult CommandJournal::acknowledge(CommandAck ack) {
    auto found = entries_.find(ack.command_id);
    if (found == entries_.end()) {
        return JournalAckResult{false, CommandState::pending, "ack did not match a journal entry"};
    }
    auto& entry = found->second;
    if (entry.command.device != ack.device) {
        return JournalAckResult{false, entry.state, "ack device did not match command device"};
    }
    if (terminal(entry.state)) {
        return JournalAckResult{true, entry.state, "ack matched a terminal command"};
    }

    entry.ack = std::move(ack);
    switch (entry.ack->status) {
    case AckStatus::accepted:
        entry.state = CommandState::acknowledged;
        note(entry, "command acknowledged");
        ++stats_.acknowledged;
        return JournalAckResult{true, entry.state, "acknowledged"};
    case AckStatus::busy:
        entry.state = CommandState::pending;
        entry.next_attempt_time_ns = entry.ack->time_ns + retry_delay(entry.attempts + 1);
        note(entry, "device busy; command rescheduled");
        return JournalAckResult{true, entry.state, "device busy"};
    case AckStatus::rejected:
    case AckStatus::malformed:
    case AckStatus::unknown_command:
        entry.state = CommandState::failed;
        note(entry, "command failed by acknowledgement");
        ++stats_.failed;
        return JournalAckResult{true, entry.state, "command failed"};
    }
    return JournalAckResult{true, entry.state, "ack processed"};
}

bool CommandJournal::cancel(CommandId id, std::string reason) {
    auto found = entries_.find(id);
    if (found == entries_.end()) {
        return false;
    }
    auto& entry = found->second;
    if (terminal(entry.state)) {
        return false;
    }
    entry.state = CommandState::cancelled;
    note(entry, std::move(reason));
    ++stats_.cancelled;
    return true;
}

std::vector<JournalEntry> CommandJournal::expire(std::uint64_t now_ns) {
    std::vector<JournalEntry> expired;
    for (auto& [unused, entry] : entries_) {
        (void)unused;
        if (terminal(entry.state)) {
            continue;
        }
        if (entry.command.expires_time_ns != 0 && now_ns >= entry.command.expires_time_ns) {
            entry.state = CommandState::expired;
            note(entry, "command expired");
            expired.push_back(entry);
            ++stats_.expired;
        }
    }
    return expired;
}

void CommandJournal::compact_completed(std::size_t keep_recent) {
    if (entries_.size() <= keep_recent) {
        return;
    }
    std::vector<CommandId> terminal_ids;
    terminal_ids.reserve(entries_.size());
    for (const auto& [id, entry] : entries_) {
        if (terminal(entry.state)) {
            terminal_ids.push_back(id);
        }
    }
    const auto removable = entries_.size() > keep_recent ? entries_.size() - keep_recent : 0;
    std::size_t removed = 0;
    for (CommandId id : terminal_ids) {
        if (removed >= removable) {
            break;
        }
        entries_.erase(id);
        ++removed;
    }
}

void CommandJournal::clear() {
    entries_.clear();
    stats_ = {};
    next_id_ = 1;
}

CommandId CommandJournal::next_id() {
    return next_id_++;
}

bool CommandJournal::duplicate_active_command(const ControlCommand& command) const {
    for (const auto& [unused, entry] : entries_) {
        (void)unused;
        if (terminal(entry.state)) {
            continue;
        }
        if (same_active_intent(entry.command, command)) {
            return true;
        }
    }
    return false;
}

protocol::Packet CommandJournal::make_packet(const JournalEntry& entry) const {
    protocol::Packet packet;
    packet.kind = protocol::PacketKind::control;
    packet.device = entry.command.device;
    packet.timestamp_ns = entry.last_send_time_ns;
    packet.sequence = static_cast<std::uint32_t>(entry.command.id & 0xffffffffu);
    packet.payload = encode_command_payload(entry.command.id, entry.command.kind, entry.command.payload);
    protocol::RoutingInfo route;
    route.collector = static_cast<std::uint16_t>(config_.controller_device & 0xffffu);
    route.priority = 255;
    packet.route = route;
    return packet;
}

std::uint64_t CommandJournal::retry_delay(std::uint32_t attempts) const {
    if (attempts == 0) {
        return config_.retry.initial_delay_ns;
    }
    std::uint64_t delay = config_.retry.initial_delay_ns;
    for (std::uint32_t i = 1; i < attempts; ++i) {
        delay = saturating_multiply(delay, config_.retry.multiplier, config_.retry.max_delay_ns);
    }
    return std::min(delay, config_.retry.max_delay_ns);
}

bool CommandJournal::terminal(CommandState state) const noexcept {
    return state == CommandState::acknowledged
        || state == CommandState::failed
        || state == CommandState::expired
        || state == CommandState::cancelled;
}

JournalEntry& CommandJournal::require_entry(CommandId id) {
    auto found = entries_.find(id);
    if (found == entries_.end()) {
        throw std::out_of_range("command journal entry not found");
    }
    return found->second;
}

void CommandJournal::note(JournalEntry& entry, std::string message) {
    entry.notes.push_back(std::move(message));
}

ControlCommand make_configure_command(protocol::DeviceId device,
                                      std::uint64_t now_ns,
                                      Bytes payload,
                                      std::string label) {
    ControlCommand command;
    command.kind = CommandKind::configure;
    command.device = device;
    command.created_time_ns = now_ns;
    command.payload = std::move(payload);
    command.label = std::move(label);
    return command;
}

ControlCommand make_start_stream_command(protocol::DeviceId device,
                                         std::uint64_t now_ns,
                                         Bytes payload) {
    ControlCommand command;
    command.kind = CommandKind::start_stream;
    command.device = device;
    command.created_time_ns = now_ns;
    command.payload = std::move(payload);
    command.label = "start_stream";
    return command;
}

ControlCommand make_stop_stream_command(protocol::DeviceId device,
                                        std::uint64_t now_ns,
                                        Bytes payload) {
    ControlCommand command;
    command.kind = CommandKind::stop_stream;
    command.device = device;
    command.created_time_ns = now_ns;
    command.payload = std::move(payload);
    command.label = "stop_stream";
    return command;
}

std::string command_kind_name(CommandKind kind) {
    switch (kind) {
    case CommandKind::configure:
        return "configure";
    case CommandKind::start_stream:
        return "start_stream";
    case CommandKind::stop_stream:
        return "stop_stream";
    case CommandKind::calibrate:
        return "calibrate";
    case CommandKind::reset:
        return "reset";
    case CommandKind::custom:
        return "custom";
    }
    return "unknown";
}

std::string command_state_name(CommandState state) {
    switch (state) {
    case CommandState::pending:
        return "pending";
    case CommandState::sent:
        return "sent";
    case CommandState::acknowledged:
        return "acknowledged";
    case CommandState::failed:
        return "failed";
    case CommandState::expired:
        return "expired";
    case CommandState::cancelled:
        return "cancelled";
    }
    return "unknown";
}

std::string ack_status_name(AckStatus status) {
    switch (status) {
    case AckStatus::accepted:
        return "accepted";
    case AckStatus::rejected:
        return "rejected";
    case AckStatus::busy:
        return "busy";
    case AckStatus::malformed:
        return "malformed";
    case AckStatus::unknown_command:
        return "unknown_command";
    }
    return "unknown";
}

std::string render_journal_entry(const JournalEntry& entry) {
    std::ostringstream out;
    out << "journal_entry\n"
        << "  id: "
        << entry.command.id
        << "\n"
        << "  device: "
        << entry.command.device
        << "\n"
        << "  kind: "
        << command_kind_name(entry.command.kind)
        << "\n"
        << "  state: "
        << command_state_name(entry.state)
        << "\n"
        << "  attempts: "
        << entry.attempts
        << "\n"
        << "  created_time_ns: "
        << entry.command.created_time_ns
        << "\n"
        << "  expires_time_ns: "
        << entry.command.expires_time_ns
        << "\n"
        << "  last_send_time_ns: "
        << entry.last_send_time_ns
        << "\n"
        << "  next_attempt_time_ns: "
        << entry.next_attempt_time_ns
        << "\n";
    if (!entry.command.label.empty()) {
        out << "  label: "
            << entry.command.label
            << "\n";
    }
    if (entry.ack) {
        out << "  ack: "
            << ack_status_name(entry.ack->status)
            << " time_ns="
            << entry.ack->time_ns
            << " message=\""
            << entry.ack->message
            << "\"\n";
    }
    for (const auto& note : entry.notes) {
        out << "  note: "
            << note
            << "\n";
    }
    return out.str();
}

std::string render_command_journal_stats(const CommandJournalStats& stats) {
    std::ostringstream out;
    out << "command_journal_stats\n"
        << "  appended: "
        << stats.appended
        << "\n"
        << "  dispatched: "
        << stats.dispatched
        << "\n"
        << "  acknowledged: "
        << stats.acknowledged
        << "\n"
        << "  failed: "
        << stats.failed
        << "\n"
        << "  expired: "
        << stats.expired
        << "\n"
        << "  cancelled: "
        << stats.cancelled
        << "\n"
        << "  duplicate_rejections: "
        << stats.duplicate_rejections
        << "\n";
    return out.str();
}

} // namespace aethon::control
