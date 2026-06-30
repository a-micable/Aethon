#pragma once

#include "aethon/config/config_parser.hpp"
#include "aethon/config/schema.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aethon::config {

struct ArchiveProfile {
    bool enabled = false;
    std::filesystem::path path;
    std::uint64_t rotate_bytes = 128 * 1024 * 1024;
};

struct StreamProfile {
    std::uint64_t max_packet_size = 16 * 1024 * 1024;
    std::uint64_t idle_timeout_ns = 30'000'000'000ULL;
};

struct RoutingProfile {
    std::uint16_t region = 0;
    std::uint8_t default_priority = 0;
};

struct CollectorProfile {
    std::string name;
    ArchiveProfile archive;
    StreamProfile stream;
    RoutingProfile routing;
    std::vector<std::string> warnings;
};

[[nodiscard]] CollectorProfile load_collector_profile(const ParsedConfig& config);
[[nodiscard]] CollectorProfile load_collector_profile_file(const std::filesystem::path& path);
[[nodiscard]] std::string render_collector_profile(const CollectorProfile& profile);

} // namespace aethon::config
