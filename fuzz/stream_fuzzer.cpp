#include "aethon/stream/stream_parser.hpp"
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) { aethon::stream::StreamParser parser({64 * 1024, false}); parser.on_packet([](aethon::protocol::Packet) {}); parser.on_error([](const aethon::Error&) {}); parser.push({data, size}); return 0; }
