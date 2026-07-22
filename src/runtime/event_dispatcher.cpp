#include "aethon/runtime/event_dispatcher.hpp"

#include "aethon/protocol/inspector.hpp"

#include <sstream>
#include <utility>

namespace aethon::runtime {

void EventDispatcher::subscribe(RuntimeEventHandler handler) {
    handlers_.push_back(std::move(handler));
}

void EventDispatcher::publish(const RuntimeEvent& event) const {
    auto handlers = handlers_;
    for (const auto& handler : handlers) {
        handler(event);
    }
}

std::size_t EventDispatcher::subscriber_count() const noexcept {
    return handlers_.size();
}

void EventDispatcher::clear() {
    handlers_.clear();
}

std::string runtime_event_kind_name(RuntimeEventKind kind) {
    switch (kind) {
    case RuntimeEventKind::packet_received:
        return "packet_received";
    case RuntimeEventKind::packet_archived:
        return "packet_archived";
    case RuntimeEventKind::session_event:
        return "session_event";
    case RuntimeEventKind::warning:
        return "warning";
    case RuntimeEventKind::error:
        return "error";
    }
    return "unknown";
}

std::string render_runtime_event(const RuntimeEvent& event) {
    std::ostringstream out;
    out << "runtime_event "
        << runtime_event_kind_name(event.kind)
        << " time_ns="
        << event.time_ns
        << " device="
        << event.packet.device
        << " sequence="
        << event.packet.sequence;
    if (!event.message.empty()) {
        out << " message=\""
            << event.message
            << "\"";
    }
    return out.str();
}

} // namespace aethon::runtime
