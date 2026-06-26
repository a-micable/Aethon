#include "aethon/protocol/packet.hpp"
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) { try { auto p = aethon::protocol::decode_packet({data, size}, {64 * 1024, false}); auto encoded = aethon::protocol::encode_packet(p); (void)aethon::protocol::decode_packet(encoded); } catch (...) {} return 0; }
