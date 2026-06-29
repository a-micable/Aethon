#include "aethon/storage/repair.hpp"

#include "aethon/codec/binary_reader.hpp"
#include "aethon/common/error.hpp"
#include "aethon/storage/archive.hpp"

#include <fstream>

namespace aethon::storage {
namespace {

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw Error(ErrorCode::archive_corrupt, "failed to open file for repair scan");
    }
    in.seekg(0, std::ios::end);
    auto size = in.tellg();
    if (size < 0) {
        throw Error(ErrorCode::archive_corrupt, "failed to determine file size");
    }
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(bytes.data()), size);
    if (in.gcount() != size) {
        throw Error(ErrorCode::archive_corrupt, "failed to read file for repair scan");
    }
    return bytes;
}

std::uint32_t read_u32_le(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint64_t infer_capture_time(std::span<const std::uint8_t> bytes, std::size_t frame_offset) {
    if (frame_offset < 12) {
        return 0;
    }
    auto prefix_offset = frame_offset - 12;
    auto recorded_len = read_u32_le(bytes, prefix_offset + 8);
    auto body_len = read_u32_le(bytes, frame_offset + 4);
    if (recorded_len != body_len + 12) {
        return 0;
    }
    codec::BinaryReader reader(bytes.subspan(prefix_offset, 8));
    return reader.u64();
}

} // namespace

ArchiveRepairScanner::ArchiveRepairScanner(protocol::DecodeOptions options)
    : options_(options) {}

RepairReport ArchiveRepairScanner::scan_file(const std::filesystem::path& path) const {
    auto bytes = read_file(path);
    return scan_bytes(bytes);
}

RepairReport ArchiveRepairScanner::scan_bytes(std::span<const std::uint8_t> bytes) const {
    RepairReport report;
    report.bytes_scanned = bytes.size();
    if (bytes.size() < 12) {
        return report;
    }

    for (std::size_t offset = 0; offset + 12 <= bytes.size(); ++offset) {
        if (read_u32_le(bytes, offset) != protocol::packet_magic) {
            continue;
        }
        ++report.candidate_frames;
        auto body_len = read_u32_le(bytes, offset + 4);
        auto frame_len = static_cast<std::size_t>(body_len) + 12;
        if (frame_len > options_.max_packet_size || offset + frame_len > bytes.size()) {
            ++report.rejected_frames;
            continue;
        }
        try {
            auto frame = bytes.subspan(offset, frame_len);
            auto packet = protocol::decode_packet(frame, options_);
            report.packets.push_back(SalvagedPacket{
                static_cast<std::uint64_t>(offset),
                infer_capture_time(bytes, offset),
                std::move(packet),
            });
            offset += frame_len - 1;
        } catch (const Error&) {
            ++report.rejected_frames;
        }
    }
    return report;
}

void write_repaired_archive(const std::filesystem::path& path, const RepairReport& report) {
    ArchiveWriter writer(path);
    for (const auto& item : report.packets) {
        auto capture_time = item.inferred_capture_time_ns == 0
            ? item.packet.timestamp_ns
            : item.inferred_capture_time_ns;
        writer.append(capture_time, item.packet);
    }
    writer.close();
}

} // namespace aethon::storage
