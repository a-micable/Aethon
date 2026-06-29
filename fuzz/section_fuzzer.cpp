#include "aethon/protocol/packet.hpp"

#include <cstddef>
#include <cstdint>
#include <numeric>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    aethon::protocol::Packet packet;
    try {
        aethon::protocol::DecodeOptions relaxed;
        relaxed.max_packet_size = 128 * 1024;
        relaxed.require_known_extensions = false;
        aethon::protocol::decode_payload_sections(packet, {data, size}, relaxed);
        std::size_t extension_bytes = 0;
        for (const auto& ext : packet.extensions) {
            extension_bytes += ext.value.size();
        }
        packet.payload.assign(data, data + size);
        auto encoded = aethon::protocol::encode_packet(packet);
        (void)aethon::protocol::decode_packet(encoded);
        (void)extension_bytes;
    } catch (...) {
    }
    return 0;
}
