#include "aethon/compression/compressor.hpp"
#include "aethon/crypto/crypto.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        auto mode = (size > 0 && (data[0] & 1)) ? aethon::protocol::CompressionMode::rle
                                                : aethon::protocol::CompressionMode::none;
        auto compressor = aethon::compression::make_compressor(mode);
        auto compressed = compressor->compress({data, size});
        auto restored = compressor->decompress(compressed, 256 * 1024);

        aethon::crypto::KeyMaterial key{{0x10, 0x20, 0x30, 0x40, 0x50}, static_cast<std::uint32_t>(size)};
        auto cipher = aethon::crypto::make_cipher(aethon::protocol::EncryptionMode::envelope);
        auto sealed = cipher->seal(restored, key);
        auto opened = cipher->open(sealed, key);

        std::uint64_t checksum = 0;
        for (auto byte : opened) {
            checksum = (checksum * 131) ^ byte;
        }
        (void)checksum;
    } catch (...) {
    }
    return 0;
}
