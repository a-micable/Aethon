#include "aethon/storage/archive.hpp"
#include "aethon/codec/binary_reader.hpp"
#include "aethon/codec/binary_writer.hpp"
#include "aethon/codec/crc.hpp"
#include "aethon/common/error.hpp"
#include <algorithm>
#include <array>
#include <limits>
namespace aethon::storage { namespace { constexpr std::array<std::uint8_t, 8> archive_magic{'A','E','T','H','A','R','C','1'}; constexpr std::uint64_t archive_record_crc_bytes = 4; void write_all(std::ofstream& out, std::span<const std::uint8_t> bytes) { out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())); if (!out) throw Error(ErrorCode::archive_corrupt, "failed to write archive data"); } Bytes read_exact(std::ifstream& in, std::size_t n) { if (n > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) throw Error(ErrorCode::archive_corrupt, "archive read size exceeds platform stream limit"); Bytes out(n); in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(n)); if (in.gcount() != static_cast<std::streamsize>(n)) throw Error(ErrorCode::archive_corrupt, "truncated archive record"); return out; } void validate_record_extent(std::ifstream& in, std::uint32_t frame_len) { if (frame_len > protocol::DecodeOptions{}.max_packet_size) throw Error(ErrorCode::archive_corrupt, "archive record frame exceeds packet size limit"); const auto frame_start = in.tellg(); in.seekg(0, std::ios::end); const auto file_end = in.tellg(); in.seekg(frame_start, std::ios::beg); const auto invalid_pos = std::ifstream::pos_type(std::streamoff(-1)); if (frame_start == invalid_pos || file_end == invalid_pos || file_end < frame_start) throw Error(ErrorCode::archive_corrupt, "archive record frame position is invalid"); const auto remaining = static_cast<std::uint64_t>(file_end - frame_start); if (remaining < static_cast<std::uint64_t>(frame_len) + archive_record_crc_bytes) throw Error(ErrorCode::archive_corrupt, "archive record is truncated"); } }
ArchiveWriter::ArchiveWriter(const std::filesystem::path& path) : out_(path, std::ios::binary | std::ios::trunc) { if (!out_) throw Error(ErrorCode::archive_corrupt, "failed to open archive for writing"); write_all(out_, archive_magic); codec::BinaryWriter header; header.u32(summary_.format_version); header.u64(0); header.u64(0); header.u64(0); write_all(out_, header.buffer()); }
ArchiveWriter::~ArchiveWriter() { if (!closed_) { try { close(); } catch (...) {} } }
void ArchiveWriter::append(std::uint64_t capture_time_ns, const protocol::Packet& packet) { auto frame = protocol::encode_packet(packet); codec::BinaryWriter record; record.u64(capture_time_ns); record.u32(static_cast<std::uint32_t>(frame.size())); record.bytes(frame); record.u32(codec::crc32c(record.buffer())); write_all(out_, record.buffer()); if (summary_.record_count == 0) summary_.first_time_ns = capture_time_ns; summary_.last_time_ns = capture_time_ns; ++summary_.record_count; }
void ArchiveWriter::close() { if (closed_) return; out_.seekp(static_cast<std::streamoff>(archive_magic.size()), std::ios::beg); codec::BinaryWriter header; header.u32(summary_.format_version); header.u64(summary_.record_count); header.u64(summary_.first_time_ns); header.u64(summary_.last_time_ns); write_all(out_, header.buffer()); out_.close(); closed_ = true; }
ArchiveReader::ArchiveReader(const std::filesystem::path& path) : in_(path, std::ios::binary) { if (!in_) throw Error(ErrorCode::archive_corrupt, "failed to open archive for reading"); auto magic = read_exact(in_, archive_magic.size()); if (!std::equal(magic.begin(), magic.end(), archive_magic.begin())) throw Error(ErrorCode::archive_corrupt, "archive magic mismatch"); auto header = read_exact(in_, 28); codec::BinaryReader r(header); summary_.format_version = r.u32(); summary_.record_count = r.u64(); summary_.first_time_ns = r.u64(); summary_.last_time_ns = r.u64(); }
std::optional<ArchiveRecord> ArchiveReader::next() {
    if (in_.peek() == std::char_traits<char>::eof()) return std::nullopt;
    ArchiveRecord rec;
    rec.offset = static_cast<std::uint64_t>(in_.tellg());
    auto prefix = read_exact(in_, 12);
    codec::BinaryReader pre(prefix);
    rec.capture_time_ns = pre.u64();
    auto frame_len = pre.u32();
    validate_record_extent(in_, frame_len);
    auto frame = read_exact(in_, frame_len);
    auto crc_bytes = read_exact(in_, 4);
    Bytes crc_input(prefix);
    crc_input.insert(crc_input.end(), frame.begin(), frame.end());
    codec::BinaryReader cr(crc_bytes);
    if (codec::crc32c(crc_input) != cr.u32()) throw Error(ErrorCode::checksum_mismatch, "archive record crc mismatch");
    rec.packet = protocol::decode_packet(frame);
    return rec;
}

ArchiveIndex build_archive_index(const std::filesystem::path& path) {
    ArchiveReader reader(path);
    ArchiveIndex index;
    index.summary.format_version = reader.summary().format_version;
    while (auto record = reader.next()) {
        index.records.push_back(ArchiveIndexEntry{
            record->capture_time_ns,
            record->offset,
            record->packet.payload.size(),
            record->packet.device,
            record->packet.sequence,
        });
    }
    index.summary.record_count = index.records.size();
    if (!index.records.empty()) {
        index.summary.first_time_ns = index.records.front().capture_time_ns;
        index.summary.last_time_ns = index.records.back().capture_time_ns;
    }
    return index;
}

std::optional<ArchiveIndexEntry> find_record_at_or_after(const ArchiveIndex& index, std::uint64_t capture_time_ns) {
    auto it = std::lower_bound(
        index.records.begin(),
        index.records.end(),
        capture_time_ns,
        [](const ArchiveIndexEntry& entry, std::uint64_t ts) {
            return entry.capture_time_ns < ts;
        });
    if (it == index.records.end()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace aethon::storage
