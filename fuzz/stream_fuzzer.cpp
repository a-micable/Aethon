#include "aethon/stream/stream_parser.hpp"
#include "aethon/protocol/reassembler.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    aethon::stream::StreamParser parser({128 * 1024, false});
    aethon::protocol::FragmentReassembler reassembler(256 * 1024);
    std::uint64_t decoded = 0;
    std::uint64_t errors = 0;
    parser.on_packet([&](aethon::protocol::Packet packet) {
        decoded += packet.sequence + packet.payload.size();
        if (auto assembled = reassembler.push(std::move(packet))) {
            (void)aethon::protocol::encode_packet(*assembled);
        }
    });
    parser.on_error([&](const aethon::Error& error) {
        errors += static_cast<unsigned>(error.code()) + 1;
    });
    if (size > 2) {
        parser.push({data, size / 2});
        parser.push({data + size / 2, size - size / 2});
    } else {
        parser.push({data, size});
    }
    (void)decoded;
    (void)errors;
    return 0;
}
