#include "aethon/protocol/packet.hpp"
#include "aethon/compression/compressor.hpp"
#include "aethon/crypto/crypto.hpp"
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        auto packet = aethon::protocol::decode_packet({data, size}, {128 * 1024, false});
        auto compressor = aethon::compression::make_compressor(packet.compression);
        auto payload = compressor->decompress(packet.payload, 256 * 1024);
        auto cipher = aethon::crypto::make_cipher(packet.encryption);
        aethon::crypto::KeyMaterial key{{0x41, 0x45, 0x54, 0x48}, packet.sequence};
        auto sealed = cipher->seal(payload, key);
        auto opened = cipher->open(sealed, key);
        packet.payload = std::move(opened);
        auto encoded = aethon::protocol::encode_packet(packet);
        (void)aethon::protocol::decode_packet(encoded, {128 * 1024, true});
    } catch (...) {
    }
    return 0;
}
