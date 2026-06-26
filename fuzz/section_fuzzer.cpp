#include "aethon/protocol/packet.hpp"
#include "aethon/protocol/extension_registry.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    aethon::protocol::Packet packet;
    try {
        aethon::protocol::DecodeOptions relaxed;
        relaxed.max_packet_size = 128 * 1024;
        relaxed.require_known_extensions = false;
        aethon::protocol::decode_payload_sections(packet, {data, size}, relaxed);
        aethon::protocol::ExtensionRegistry registry("section-fuzzer");
        for (const auto& ext : packet.extensions) {
            registry.observe({ext.type, static_cast<double>(ext.value.size()), 1.0, "extension"});
        }
        packet.payload.assign(data, data + size);
        auto encoded = aethon::protocol::encode_packet(packet);
        (void)aethon::protocol::decode_packet(encoded);
        (void)registry.summarize();
    } catch (...) {
    }
    return 0;
}
