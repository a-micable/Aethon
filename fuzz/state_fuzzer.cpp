#include "aethon/codec/binary_reader.hpp"
#include "aethon/codec/binary_writer.hpp"
#include "aethon/config/config_parser.hpp"
#include "aethon/protocol/packet.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        aethon::codec::BinaryReader reader({data, size});
        aethon::codec::BinaryWriter writer;
        std::size_t steps = 0;
        while (!reader.empty() && steps++ < 256) {
            auto selector = reader.u8() % 5;
            if (selector == 0 && reader.remaining() >= 2) {
                writer.u16(reader.u16());
            } else if (selector == 1 && reader.remaining() >= 4) {
                writer.u32(reader.u32());
            } else if (selector == 2 && reader.remaining() >= 8) {
                writer.u64(reader.u64());
            } else if (selector == 3 && reader.remaining() >= 1) {
                auto n = static_cast<std::size_t>(reader.u8() % 16);
                if (reader.remaining() >= n) {
                    writer.bytes(reader.bytes(n));
                }
            } else {
                writer.u8(selector);
            }
        }
        auto rebuilt = writer.take();
        (void)aethon::protocol::decode_packet(rebuilt, {128 * 1024, false});
    } catch (...) {
    }

    try {
        aethon::config::ConfigParser parser;
        auto parsed = parser.parse_bytes({data, size});
        (void)parsed.get("collector.name");
    } catch (...) {
    }
    return 0;
}
