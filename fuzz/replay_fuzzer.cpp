#include "aethon/replay/replay_engine.hpp"
#include "fuzz_io.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    auto path = aethon::fuzz::write_temp_input("aethon-replay-fuzz", data, size);
    try {
        aethon::storage::ArchiveReader reader(path);
        aethon::replay::ReplayOptions options;
        options.speed = 1.0 + static_cast<double>(size % 7);
        options.start_time_ns = size > 0 ? data[0] : 0;
        options.end_time_ns = size > 1 ? 4096 + data[1] : 0;
        std::uint64_t emitted_payload_bytes = 0;
        aethon::replay::ReplayEngine engine(options);
        (void)engine.run(reader, [&](const aethon::storage::ArchiveRecord& record) {
            emitted_payload_bytes += record.packet.payload.size();
            auto frame = aethon::protocol::encode_packet(record.packet);
            (void)aethon::protocol::decode_packet(frame);
        });
        (void)emitted_payload_bytes;
    } catch (...) {
    }
    std::filesystem::remove(path);
    return 0;
}
