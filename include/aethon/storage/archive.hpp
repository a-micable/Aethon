#pragma once
#include "aethon/protocol/packet.hpp"
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>
namespace aethon::storage {
struct ArchiveRecord { std::uint64_t offset = 0; std::uint64_t capture_time_ns = 0; protocol::Packet packet; };
struct ArchiveSummary { std::uint32_t format_version = 1; std::uint64_t record_count = 0; std::uint64_t first_time_ns = 0; std::uint64_t last_time_ns = 0; };
struct ArchiveIndexEntry { std::uint64_t capture_time_ns = 0; std::uint64_t offset = 0; std::uint64_t payload_size = 0; protocol::DeviceId device = 0; std::uint32_t sequence = 0; };
struct ArchiveIndex { ArchiveSummary summary; std::vector<ArchiveIndexEntry> records; };
class ArchiveWriter { public: explicit ArchiveWriter(const std::filesystem::path& path); ~ArchiveWriter(); void append(std::uint64_t capture_time_ns, const protocol::Packet& packet); [[nodiscard]] const ArchiveSummary& summary() const noexcept { return summary_; } void close(); private: std::ofstream out_; ArchiveSummary summary_; bool closed_ = false; };
class ArchiveReader { public: explicit ArchiveReader(const std::filesystem::path& path); [[nodiscard]] const ArchiveSummary& summary() const noexcept { return summary_; } [[nodiscard]] std::optional<ArchiveRecord> next(); private: std::ifstream in_; ArchiveSummary summary_; };
ArchiveIndex build_archive_index(const std::filesystem::path& path);
std::optional<ArchiveIndexEntry> find_record_at_or_after(const ArchiveIndex& index, std::uint64_t capture_time_ns);
} // namespace aethon::storage
