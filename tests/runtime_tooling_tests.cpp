#include "test_harness.hpp"

#include "aethon/diagnostics/json_writer.hpp"
#include "aethon/protocol/batch_codec.hpp"
#include "aethon/protocol/packet_builder.hpp"
#include "aethon/storage/query_parser.hpp"
#include "aethon/stream/session_tracker.hpp"
#include "aethon/telemetry/payload.hpp"
#include "aethon/telemetry/statistics.hpp"

AETHON_TEST(telemetry_payload_and_statistics_round_trip) {
    aethon::telemetry::ScalarObservation observation;
    observation.sample_time_ns = 100;
    observation.sample_rate_hz = 10;
    observation.readings.push_back({7, 123, 0, aethon::telemetry::ReadingQuality::good});

    auto bytes = aethon::telemetry::encode_scalar_observation(observation);
    auto decoded = aethon::telemetry::decode_scalar_observation(bytes);
    auto stats = aethon::telemetry::analyze_scalar_observation(decoded);

    AETHON_REQUIRE(decoded.readings.size() == 1);
    AETHON_REQUIRE(stats.total_readings == 1);
}

AETHON_TEST(query_parser_accepts_named_packet_kinds) {
    auto parsed = aethon::storage::parse_archive_query("device=42 kind=spectrum min_payload=4 limit=2");

    AETHON_REQUIRE(parsed.ok());
    AETHON_REQUIRE(parsed.query.device.value() == 42);
    AETHON_REQUIRE(parsed.query.limit == 2);
}

AETHON_TEST(packet_batch_codec_round_trips_builder_packets) {
    auto packet = aethon::protocol::make_observation(9, 1000, 1, {1, 2, 3});
    aethon::protocol::PacketBatch batch;
    batch.packets.push_back(packet);

    auto encoded = aethon::protocol::encode_packet_batch(batch);
    auto decoded = aethon::protocol::decode_packet_batch(encoded);

    AETHON_REQUIRE(decoded.packets.size() == 1);
    AETHON_REQUIRE(decoded.packets.front().device == 9);
}

AETHON_TEST(session_tracker_reports_sequence_gaps) {
    aethon::stream::SessionTracker tracker({1000, 1});
    auto first = aethon::protocol::make_heartbeat(11, 10, 1);
    auto second = aethon::protocol::make_heartbeat(11, 20, 5);

    auto opened = tracker.observe(first, 10);
    auto events = tracker.observe(second, 20);
    auto snapshot = tracker.snapshot();

    AETHON_REQUIRE(!opened.empty());
    AETHON_REQUIRE(!events.empty());
    AETHON_REQUIRE(snapshot.total_gaps == 1);
}

AETHON_TEST(json_writer_renders_status_objects) {
    auto json = aethon::diagnostics::render_status_json("ok", "ready", {"checked"});

    AETHON_REQUIRE(json.find("\"status\":\"ok\"") != std::string::npos);
    AETHON_REQUIRE(json.find("\"notes\"") != std::string::npos);
}
