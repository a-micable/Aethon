#pragma once
#include "aethon/protocol/packet.hpp"
#include <filesystem>
#include <fstream>
#include <optional>
namespace aethon::storage {
struct ArchiveRecord { std::uint64_t offset = 0; std::uint64_t capture_time_ns = 0; protocol::Packet packet; };
struct ArchiveSummary { std::uint32_t format_version = 1; std::uint64_t record_count = 0; std::uint64_t first_time_ns = 0; std::uint64_t last_time_ns = 0; };
class ArchiveWriter { public: explicit ArchiveWriter(const std::filesystem::path& path); ~ArchiveWriter(); void append(std::uint64_t capture_time_ns, const protocol::Packet& packet); [[nodiscard]] const ArchiveSummary& summary() const noexcept { return summary_; } void close(); private: std::ofstream out_; ArchiveSummary summary_; bool closed_ = false; };
class ArchiveReader { public: explicit ArchiveReader(const std::filesystem::path& path); [[nodiscard]] const ArchiveSummary& summary() const noexcept { return summary_; } [[nodiscard]] std::optional<ArchiveRecord> next(); private: std::ifstream in_; ArchiveSummary summary_; };
} // namespace aethon::storage
