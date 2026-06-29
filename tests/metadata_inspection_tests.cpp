#include "test_harness.hpp"

#include "aethon/config/schema.hpp"
#include "aethon/protocol/inspector.hpp"
#include "aethon/protocol/tlv.hpp"

AETHON_TEST(tlv_parser_round_trips_strings_and_integers) {
    auto name = aethon::protocol::make_tlv_string(10, "east collector");
    auto region = aethon::protocol::make_tlv_u64(11, 7);
    name.insert(name.end(), region.begin(), region.end());

    auto fields = aethon::protocol::parse_tlv_fields(name);
    auto name_field = aethon::protocol::find_tlv_field(fields, 10);
    auto region_field = aethon::protocol::find_tlv_field(fields, 11);

    AETHON_REQUIRE(name_field.has_value());
    AETHON_REQUIRE(region_field.has_value());
    AETHON_REQUIRE(aethon::protocol::tlv_string(*name_field).value() == "east collector");
    AETHON_REQUIRE(aethon::protocol::tlv_unsigned(*region_field).value() == 7);
    AETHON_REQUIRE(aethon::protocol::encode_tlv_fields(fields) == name);
}

AETHON_TEST(packet_inspector_reports_metadata_and_warnings) {
    aethon::protocol::Packet packet;
    packet.kind = aethon::protocol::PacketKind::spectrum;
    packet.device = 0x42;
    packet.sequence = 100;
    packet.route = aethon::protocol::RoutingInfo{7, 12, 4, {1, 2}};
    packet.extensions.push_back({900, {1, 2, 3, 4}});

    auto inspection = aethon::protocol::inspect_packet(packet);
    auto rendered = aethon::protocol::render_packet_inspection(inspection);

    AETHON_REQUIRE(inspection.kind == "spectrum");
    AETHON_REQUIRE(!inspection.warnings.empty());
    AETHON_REQUIRE(rendered.find("route region=7") != std::string::npos);
    AETHON_REQUIRE(rendered.find("extension type=900") != std::string::npos);
}

AETHON_TEST(config_schema_reports_missing_and_out_of_range_values) {
    aethon::config::ConfigParser parser;
    auto parsed = parser.parse(R"(
        collector.name = "east"
        collector.region = 70000
        archive.enabled = maybe
    )");

    auto validation = aethon::config::collector_config_schema().validate(parsed);

    AETHON_REQUIRE(!validation.ok());
    AETHON_REQUIRE(validation.issues.size() == 2);
}
