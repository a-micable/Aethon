#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/storage/archive.hpp"
#include "aethon/stream/session_tracker.hpp"

#include <functional>
#include <string>
#include <vector>

namespace aethon::runtime {

enum class RuntimeEventKind {
    packet_received,
    packet_archived,
    session_event,
    warning,
    error,
};

struct RuntimeEvent {
    RuntimeEventKind kind = RuntimeEventKind::packet_received;
    std::uint64_t time_ns = 0;
    std::string message;
    protocol::Packet packet;
};

using RuntimeEventHandler = std::function<void(const RuntimeEvent&)>;

class EventDispatcher {
public:
    void subscribe(RuntimeEventHandler handler);
    void publish(const RuntimeEvent& event) const;
    [[nodiscard]] std::size_t subscriber_count() const noexcept;
    void clear();

private:
    std::vector<RuntimeEventHandler> handlers_;
};

[[nodiscard]] std::string runtime_event_kind_name(RuntimeEventKind kind);
[[nodiscard]] std::string render_runtime_event(const RuntimeEvent& event);

} // namespace aethon::runtime
