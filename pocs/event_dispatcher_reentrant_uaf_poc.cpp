#include "aethon/runtime/event_dispatcher.hpp"

#include <cstdint>

int main() {
    aethon::runtime::EventDispatcher dispatcher;

    dispatcher.subscribe([&](const aethon::runtime::RuntimeEvent&) {
        for (std::uint32_t i = 0; i < 1024; ++i) {
            dispatcher.subscribe([](const aethon::runtime::RuntimeEvent&) {});
        }
    });

    dispatcher.subscribe([](const aethon::runtime::RuntimeEvent&) {});

    aethon::runtime::RuntimeEvent event;
    event.kind = aethon::runtime::RuntimeEventKind::packet_received;
    event.time_ns = 1;
    event.packet.device = 7;
    event.packet.sequence = 42;
    event.message = "trigger reentrant dispatcher mutation";

    dispatcher.publish(event);
    return 0;
}
