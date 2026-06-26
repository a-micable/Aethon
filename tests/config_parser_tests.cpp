#include "test_harness.hpp"

#include "aethon/common/error.hpp"
#include "aethon/config/config_parser.hpp"

AETHON_TEST(config_parser_handles_quotes_comments_and_types) {
    aethon::config::ConfigParser parser;
    auto config = parser.parse(R"(
        # collector profile
        collector.name = "east \"primary\""
        archive.enabled = true
        stream.max_packet_size = 65536
    )");

    AETHON_REQUIRE(config.entries.size() == 3);
    AETHON_REQUIRE(config.get("collector.name").value() == "east \"primary\"");
    AETHON_REQUIRE(config.get_bool("archive.enabled", false));
    AETHON_REQUIRE(config.get_int("stream.max_packet_size", 0) == 65536);
}

AETHON_TEST(config_parser_rejects_nul_bytes) {
    aethon::config::ConfigParser parser;
    const std::uint8_t bytes[] = {'a', '=', 'b', 0, 'c'};
    bool rejected = false;
    try {
        (void)parser.parse_bytes(bytes);
    } catch (const aethon::Error&) {
        rejected = true;
    }
    AETHON_REQUIRE(rejected);
}
