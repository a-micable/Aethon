#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/storage/query.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::storage {

struct PacketKindCount {
    protocol::PacketKind kind = protocol::PacketKind::heartbeat;
    std::uint64_t count = 0;
};

struct ArchiveManifest {
    ArchiveSummary summary;
    std::vector<DeviceArchiveSummary> devices;
    std::vector<PacketKindCount> packet_kinds;
    std::vector<std::string> warnings;
};

[[nodiscard]] ArchiveManifest build_archive_manifest(const std::filesystem::path& path);
[[nodiscard]] std::vector<std::string> validate_archive_manifest(const ArchiveManifest& manifest);
[[nodiscard]] std::string render_archive_manifest(const ArchiveManifest& manifest);

} // namespace aethon::storage
