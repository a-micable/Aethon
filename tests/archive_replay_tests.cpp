#include "test_harness.hpp"

#include "aethon/replay/replay_engine.hpp"
#include "aethon/storage/archive.hpp"
#include "aethon/storage/repair.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

aethon::protocol::Packet make_packet(std::uint64_t device, std::uint32_t sequence, std::initializer_list<std::uint8_t> payload) {
    aethon::protocol::Packet packet;
    packet.device = device;
    packet.sequence = sequence;
    packet.kind = aethon::protocol::PacketKind::observation;
    packet.payload.assign(payload.begin(), payload.end());
    return packet;
}

} // namespace

AETHON_TEST(archive_index_tracks_offsets_and_sequences) {
    auto path = std::filesystem::temp_directory_path() / "aethon_archive_index_test.ath";
    {
        aethon::storage::ArchiveWriter writer(path);
        writer.append(1000, make_packet(10, 1, {1, 2, 3}));
        writer.append(1500, make_packet(11, 2, {4, 5}));
        writer.close();
    }

    auto index = aethon::storage::build_archive_index(path);
    AETHON_REQUIRE(index.summary.record_count == 2);
    AETHON_REQUIRE(index.records.size() == 2);
    AETHON_REQUIRE(index.records[0].device == 10);
    AETHON_REQUIRE(index.records[1].sequence == 2);

    auto hit = aethon::storage::find_record_at_or_after(index, 1200);
    AETHON_REQUIRE(hit.has_value());
    AETHON_REQUIRE(hit->capture_time_ns == 1500);
    std::filesystem::remove(path);
}

AETHON_TEST(replay_engine_applies_capture_time_window) {
    auto path = std::filesystem::temp_directory_path() / "aethon_replay_window_test.ath";
    {
        aethon::storage::ArchiveWriter writer(path);
        writer.append(100, make_packet(20, 1, {1}));
        writer.append(200, make_packet(20, 2, {2, 2}));
        writer.append(300, make_packet(20, 3, {3, 3, 3}));
        writer.close();
    }

    aethon::storage::ArchiveReader reader(path);
    aethon::replay::ReplayEngine engine({1.0, 150, 250, false});
    std::uint64_t emitted_sequence_sum = 0;
    auto stats = engine.run(reader, [&](const aethon::storage::ArchiveRecord& record) {
        emitted_sequence_sum += record.packet.sequence;
    });

    AETHON_REQUIRE(stats.records_seen == 3);
    AETHON_REQUIRE(stats.records_emitted == 1);
    AETHON_REQUIRE(emitted_sequence_sum == 2);
    std::filesystem::remove(path);
}

AETHON_TEST(repair_scanner_salvages_packet_frames_from_noise) {
    auto path = std::filesystem::temp_directory_path() / "aethon_repair_scan_test.bin";
    auto packet = make_packet(30, 9, {9, 8, 7});
    auto frame = aethon::protocol::encode_packet(packet);
    {
        std::ofstream out(path, std::ios::binary);
        const std::vector<std::uint8_t> noise = {0xff, 0x00, 0x13, 0x37};
        out.write(reinterpret_cast<const char*>(noise.data()), static_cast<std::streamsize>(noise.size()));
        out.write(reinterpret_cast<const char*>(frame.data()), static_cast<std::streamsize>(frame.size()));
    }

    aethon::storage::ArchiveRepairScanner scanner;
    auto report = scanner.scan_file(path);

    AETHON_REQUIRE(report.candidate_frames == 1);
    AETHON_REQUIRE(report.packets.size() == 1);
    AETHON_REQUIRE(report.packets.front().packet.device == 30);
    std::filesystem::remove(path);
}
