#include "test_harness.hpp"

#include "aethon/protocol/packet.hpp"

AETHON_TEST(packet_round_trip_preserves_sections) {
    aethon::protocol::Packet packet;
    packet.kind = aethon::protocol::PacketKind::spectrum;
    packet.device = 42;
    packet.sequence = 9;
    packet.route = aethon::protocol::RoutingInfo{7, 12, 3, {4, 5}};
    packet.fragment = aethon::protocol::FragmentInfo{80, 1, 2};
    packet.extensions.push_back({900, {1, 2, 3}});
    packet.payload = {8, 6, 7, 5};

    auto raw = aethon::protocol::encode_packet(packet);
    auto decoded = aethon::protocol::decode_packet(raw);

    AETHON_REQUIRE(decoded.device == 42);
    AETHON_REQUIRE(decoded.route && decoded.route->collector == 12);
    AETHON_REQUIRE(decoded.fragment && decoded.fragment->sequence == 1);
    AETHON_REQUIRE(decoded.extensions.size() == 1);
    AETHON_REQUIRE(decoded.payload == packet.payload);
}
