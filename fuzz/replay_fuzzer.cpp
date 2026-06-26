#include "aethon/replay/replay_engine.hpp"
#include "aethon/routing/route_table.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    auto path = std::filesystem::temp_directory_path() / "aethon_replay_fuzz.ath";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    try {
        aethon::storage::ArchiveReader reader(path);
        aethon::replay::ReplayOptions options;
        options.speed = 1.0 + static_cast<double>(size % 7);
        options.start_time_ns = size > 0 ? data[0] : 0;
        options.end_time_ns = size > 1 ? 4096 + data[1] : 0;
        aethon::routing::RouteTable routes("replay-fuzzer");
        aethon::replay::ReplayEngine engine(options);
        (void)engine.run(reader, [&](const aethon::storage::ArchiveRecord& record) {
            routes.observe({record.capture_time_ns, static_cast<double>(record.packet.payload.size()), 1.0, "packet"});
            (void)routes.summarize();
        });
    } catch (...) {
    }
    std::filesystem::remove(path);
    return 0;
}
