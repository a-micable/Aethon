#include "test_harness.hpp"

#include "aethon/protocol/reassembler.hpp"

namespace {

aethon::protocol::Packet fragment(std::uint16_t sequence, std::initializer_list<std::uint8_t> payload) {
    aethon::protocol::Packet packet;
    packet.device = 900;
    packet.sequence = sequence + 10;
    packet.fragment = aethon::protocol::FragmentInfo{55, sequence, 3};
    packet.payload.assign(payload.begin(), payload.end());
    return packet;
}

} // namespace

AETHON_TEST(reassembler_completes_out_of_order_fragments) {
    aethon::protocol::FragmentReassembler reassembler;

    AETHON_REQUIRE(!reassembler.push(fragment(2, {5, 6})).has_value());
    AETHON_REQUIRE(!reassembler.push(fragment(0, {1, 2})).has_value());
    auto assembled = reassembler.push(fragment(1, {3, 4}));

    AETHON_REQUIRE(assembled.has_value());
    AETHON_REQUIRE(assembled->payload == std::vector<std::uint8_t>({1, 2, 3, 4, 5, 6}));
    AETHON_REQUIRE(!assembled->fragment.has_value());
    AETHON_REQUIRE(reassembler.stats().completed_messages == 1);
}

AETHON_TEST(reassembler_ignores_duplicate_fragments) {
    aethon::protocol::FragmentReassembler reassembler;

    AETHON_REQUIRE(!reassembler.push(fragment(0, {1})).has_value());
    AETHON_REQUIRE(!reassembler.push(fragment(0, {9})).has_value());
    AETHON_REQUIRE(reassembler.stats().duplicate_fragments == 1);
}
