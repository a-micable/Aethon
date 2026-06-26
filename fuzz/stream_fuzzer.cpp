#include "aethon/stream/stream_parser.hpp"
#include "aethon/integrity/sequence_tracker.hpp"
#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    aethon::stream::StreamParser parser({128 * 1024, false});
    aethon::integrity::SequenceTracker tracker("stream-fuzzer");
    parser.on_packet([&](aethon::protocol::Packet packet) {
        tracker.observe({packet.timestamp_ns, static_cast<double>(packet.sequence), 1.0, "decoded"});
        (void)aethon::protocol::encode_packet(packet);
    });
    parser.on_error([&](const aethon::Error& error) {
        tracker.observe({0, static_cast<double>(static_cast<int>(error.code())), 1.0, "error"});
    });
    if (size > 2) {
        parser.push({data, size / 2});
        parser.push({data + size / 2, size - size / 2});
    } else {
        parser.push({data, size});
    }
    (void)tracker.summarize();
    return 0;
}
