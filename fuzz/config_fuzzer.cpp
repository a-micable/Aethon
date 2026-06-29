#include "aethon/config/config_parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        aethon::config::ConfigParser parser;
        parser.set_max_line_length(2048);
        parser.set_max_entries(256);
        auto parsed = parser.parse_bytes({data, size});
        std::size_t total_value_bytes = 0;
        for (const auto& entry : parsed.entries) {
            total_value_bytes += entry.key.size();
            total_value_bytes += entry.value.size();
        }
        (void)parsed.get_bool("archive.enabled", false);
        (void)parsed.get_int("stream.max_packet_size", 1048576);
        (void)parsed.get("collector.name");
        (void)total_value_bytes;
    } catch (...) {
    }
    return 0;
}
