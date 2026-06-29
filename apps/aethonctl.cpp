#include "aethon/common/error.hpp"
#include "aethon/protocol/inspector.hpp"
#include "aethon/replay/replay_engine.hpp"
#include "aethon/storage/archive.hpp"
#include "aethon/storage/manifest.hpp"
#include "aethon/storage/repair.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage() {
    std::cout << "usage:\n"
              << "  aethonctl inspect <archive.ath>\n"
              << "  aethonctl inspect-packet <packet.bin>\n"
              << "  aethonctl manifest <archive.ath>\n"
              << "  aethonctl replay <archive.ath>\n"
              << "  aethonctl repair-scan <capture-or-archive>\n";
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw aethon::Error(aethon::ErrorCode::invalid_argument, "failed to open input file");
    }
    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    if (size < 0) {
        throw aethon::Error(aethon::ErrorCode::invalid_argument, "failed to determine input size");
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), size);
    if (in.gcount() != size) {
        throw aethon::Error(aethon::ErrorCode::invalid_argument, "failed to read input file");
    }
    return bytes;
}

int inspect(const std::filesystem::path& path) {
    aethon::storage::ArchiveReader reader(path);
    const auto& s = reader.summary();
    std::cout << "format_version=" << s.format_version << '\n'
              << "records=" << s.record_count << '\n'
              << "first_time_ns=" << s.first_time_ns << '\n'
              << "last_time_ns=" << s.last_time_ns << '\n';
    auto index = aethon::storage::build_archive_index(path);
    for (const auto& entry : index.records) {
        std::cout << "record offset=" << entry.offset
                  << " capture_time_ns=" << entry.capture_time_ns
                  << " device=" << entry.device
                  << " sequence=" << entry.sequence
                  << " payload_size=" << entry.payload_size << '\n';
    }
    return 0;
}

int inspect_packet(const std::filesystem::path& path) {
    auto bytes = read_file(path);
    auto packet = aethon::protocol::decode_packet(bytes);
    auto inspection = aethon::protocol::inspect_packet(packet);
    std::cout << aethon::protocol::render_packet_inspection(inspection);
    return 0;
}

int manifest(const std::filesystem::path& path) {
    auto manifest = aethon::storage::build_archive_manifest(path);
    std::cout << aethon::storage::render_archive_manifest(manifest);
    return manifest.warnings.empty() ? 0 : 3;
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

int repair_scan(const std::filesystem::path& path) {
    aethon::storage::ArchiveRepairScanner scanner;
    auto report = scanner.scan_file(path);
    std::cout << "bytes_scanned=" << report.bytes_scanned << '\n'
              << "candidate_frames=" << report.candidate_frames << '\n'
              << "rejected_frames=" << report.rejected_frames << '\n'
              << "salvaged_packets=" << report.packets.size() << '\n';
    for (const auto& packet : report.packets) {
        std::cout << "packet offset=" << packet.file_offset
                  << " capture_time_ns=" << packet.inferred_capture_time_ns
                  << " device=" << packet.packet.device
                  << " sequence=" << packet.packet.sequence << '\n';
    }
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
        if (command == "inspect-packet") {
            return inspect_packet(argv[2]);
        }
        if (command == "manifest") {
            return manifest(argv[2]);
        }
        if (command == "replay") {
            return replay(argv[2]);
        }
        if (command == "repair-scan") {
            return repair_scan(argv[2]);
        }
        usage();
        return 2;
    } catch (const aethon::Error& e) {
        std::cerr << "aethonctl: " << e.what() << '\n';
        return 1;
    }
}
