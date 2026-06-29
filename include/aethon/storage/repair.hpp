#pragma once

#include "aethon/protocol/packet.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace aethon::storage {

struct SalvagedPacket {
    std::uint64_t file_offset = 0;
    std::uint64_t inferred_capture_time_ns = 0;
    protocol::Packet packet;
};

struct RepairReport {
    std::uint64_t bytes_scanned = 0;
    std::uint64_t candidate_frames = 0;
    std::uint64_t rejected_frames = 0;
    std::vector<SalvagedPacket> packets;
};

class ArchiveRepairScanner {
public:
    explicit ArchiveRepairScanner(protocol::DecodeOptions options = {});

    [[nodiscard]] RepairReport scan_file(const std::filesystem::path& path) const;
    [[nodiscard]] RepairReport scan_bytes(std::span<const std::uint8_t> bytes) const;

private:
    protocol::DecodeOptions options_;
};

void write_repaired_archive(const std::filesystem::path& path, const RepairReport& report);

} // namespace aethon::storage
