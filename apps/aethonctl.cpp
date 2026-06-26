#include "aethon/common/error.hpp"
#include "aethon/replay/replay_engine.hpp"
#include "aethon/storage/archive.hpp"
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void usage() {
    std::cout << "usage:\n"
              << "  aethonctl inspect <archive.ath>\n"
              << "  aethonctl replay <archive.ath>\n";
}

int inspect(const std::filesystem::path& path) {
    aethon::storage::ArchiveReader reader(path);
    const auto& s = reader.summary();
    std::cout << "format_version=" << s.format_version << '\n'
              << "records=" << s.record_count << '\n'
              << "first_time_ns=" << s.first_time_ns << '\n'
              << "last_time_ns=" << s.last_time_ns << '\n';
    return 0;
}

int replay(const std::filesystem::path& path) {
    aethon::storage::ArchiveReader reader(path);
    aethon::replay::ReplayEngine engine;
    auto stats = engine.run(reader, [](const aethon::storage::ArchiveRecord& rec) {
        std::cout << rec.capture_time_ns << " device=" << rec.packet.device
                  << " seq=" << rec.packet.sequence
                  << " bytes=" << rec.packet.payload.size() << '\n';
    });
    std::cerr << "emitted " << stats.records_emitted << " records\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        usage();
        return 2;
    }
    try {
        std::string command = argv[1];
        if (command == "inspect") {
            return inspect(argv[2]);
        }
        if (command == "replay") {
            return replay(argv[2]);
        }
        usage();
        return 2;
    } catch (const aethon::Error& e) {
        std::cerr << "aethonctl: " << e.what() << '\n';
        return 1;
    }
}
