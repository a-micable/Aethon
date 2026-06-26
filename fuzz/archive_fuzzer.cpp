#include "aethon/storage/archive.hpp"
#include "aethon/protocol/packet.hpp"
#include "fuzz_io.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    auto path = aethon::fuzz::write_temp_input("aethon-archive-fuzz", data, size);
    try {
        aethon::storage::ArchiveReader reader(path);
        std::uint64_t records = 0;
        while (auto record = reader.next()) {
            records += record->packet.payload.size();
            if (records > 1024 * 1024) {
                break;
            }
            auto encoded = aethon::protocol::encode_packet(record->packet);
            (void)aethon::protocol::decode_packet(encoded);
        }
    } catch (...) {
    }
    std::filesystem::remove(path);
    return 0;
}
