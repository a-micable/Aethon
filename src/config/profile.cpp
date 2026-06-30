#include "aethon/config/profile.hpp"

#include "aethon/common/error.hpp"

#include <fstream>
#include <sstream>

namespace aethon::config {
namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw Error(ErrorCode::invalid_argument, "failed to open config profile");
    }
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::uint64_t get_u64_or(const ParsedConfig& config, const std::string& key, std::uint64_t fallback) {
    auto value = config.get_int(key, static_cast<int>(fallback));
    if (value < 0) {
        return fallback;
    }
    return static_cast<std::uint64_t>(value);
}

std::uint8_t get_u8_or(const ParsedConfig& config, const std::string& key, std::uint8_t fallback) {
    auto value = config.get_int(key, fallback);
    if (value < 0 || value > 255) {
        return fallback;
    }
    return static_cast<std::uint8_t>(value);
}

void append_schema_warnings(CollectorProfile& profile, const ConfigValidation& validation) {
    for (const auto& issue : validation.issues) {
        std::ostringstream warning;
        warning << issue.key << ": " << issue.message;
        profile.warnings.push_back(warning.str());
    }
}

void append_profile_warnings(CollectorProfile& profile) {
    if (profile.name.empty()) {
        profile.warnings.push_back("collector name is empty");
    }
    if (profile.archive.enabled && profile.archive.path.empty()) {
        profile.warnings.push_back("archive is enabled without an archive path");
    }
    if (profile.stream.max_packet_size < 64) {
        profile.warnings.push_back("stream max packet size is suspiciously small");
    }
    if (profile.archive.rotate_bytes < profile.stream.max_packet_size) {
        profile.warnings.push_back("archive rotation size is smaller than maximum packet size");
    }
}

} // namespace

CollectorProfile load_collector_profile(const ParsedConfig& config) {
    CollectorProfile profile;
    auto validation = collector_config_schema().validate(config);
    append_schema_warnings(profile, validation);

    profile.name = config.get("collector.name").value_or("aethon-collector");
    profile.routing.region = static_cast<std::uint16_t>(config.get_int("collector.region", 0));
    profile.routing.default_priority = get_u8_or(config, "routing.default_priority", 0);

    profile.archive.enabled = config.get_bool("archive.enabled", false);
    profile.archive.path = config.get("archive.path").value_or("capture.ath");
    profile.archive.rotate_bytes = get_u64_or(config, "archive.rotate_bytes", profile.archive.rotate_bytes);

    profile.stream.max_packet_size = get_u64_or(
        config,
        "stream.max_packet_size",
        profile.stream.max_packet_size);
    profile.stream.idle_timeout_ns = get_u64_or(
        config,
        "stream.idle_timeout_ns",
        profile.stream.idle_timeout_ns);

    append_profile_warnings(profile);
    return profile;
}

CollectorProfile load_collector_profile_file(const std::filesystem::path& path) {
    ConfigParser parser;
    return load_collector_profile(parser.parse(read_text_file(path)));
}

std::string render_collector_profile(const CollectorProfile& profile) {
    std::ostringstream out;
    out << "collector_profile\n"
        << "  name: "
        << profile.name
        << "\n"
        << "  archive_enabled: "
        << (profile.archive.enabled ? "yes" : "no")
        << "\n"
        << "  archive_path: "
        << profile.archive.path.string()
        << "\n"
        << "  archive_rotate_bytes: "
        << profile.archive.rotate_bytes
        << "\n"
        << "  stream_max_packet_size: "
        << profile.stream.max_packet_size
        << "\n"
        << "  stream_idle_timeout_ns: "
        << profile.stream.idle_timeout_ns
        << "\n"
        << "  region: "
        << profile.routing.region
        << "\n"
        << "  default_priority: "
        << static_cast<unsigned>(profile.routing.default_priority)
        << "\n";
    for (const auto& warning : profile.warnings) {
        out << "  warning: "
            << warning
            << "\n";
    }
    return out.str();
}

} // namespace aethon::config
