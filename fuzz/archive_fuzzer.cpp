#include "aethon/storage/archive.hpp"
#include "aethon/protocol/packet.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    auto path = std::filesystem::temp_directory_path() / "aethon_archive_fuzz.ath";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
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
