#include "aethon/config/config_parser.hpp"
#include "aethon/config/loader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        aethon::config::ConfigParser parser;
        parser.set_max_line_length(2048);
        parser.set_max_entries(256);
        auto parsed = parser.parse_bytes({data, size});
        aethon::config::ConfigLoader loader("config-fuzzer");
        for (const auto& entry : parsed.entries) {
            loader.observe({entry.line, static_cast<double>(entry.value.size()), 1.0, entry.key});
        }
        (void)parsed.get_bool("archive.enabled", false);
        (void)parsed.get_int("stream.max_packet_size", 1048576);
        (void)loader.summarize();
    } catch (...) {
    }
    return 0;
}
