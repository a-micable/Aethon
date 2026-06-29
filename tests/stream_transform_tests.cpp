#include "test_harness.hpp"

#include "aethon/compression/compressor.hpp"
#include "aethon/crypto/crypto.hpp"
#include "aethon/stream/stream_parser.hpp"

#include <vector>

AETHON_TEST(stream_parser_recovers_packet_after_link_noise) {
    aethon::protocol::Packet packet;
    packet.device = 77;
    packet.sequence = 9;
    packet.payload = {9, 8, 7};
    auto frame = aethon::protocol::encode_packet(packet);

    std::vector<std::uint8_t> stream = {0xff, 0x00, 0x13, 0x37};
    stream.insert(stream.end(), frame.begin(), frame.end());

    aethon::stream::StreamParser parser;
    std::uint64_t decoded = 0;
    parser.on_packet([&](aethon::protocol::Packet decoded_packet) {
        decoded += decoded_packet.device;
        AETHON_REQUIRE(decoded_packet.payload == packet.payload);
    });
    parser.push(stream);
    AETHON_REQUIRE(decoded == 77);
    AETHON_REQUIRE(parser.buffered() == 0);
}

AETHON_TEST(compression_and_envelope_cipher_round_trip_payload) {
    std::vector<std::uint8_t> input = {1, 1, 1, 2, 3, 3, 3, 3, 4};
    auto compressor = aethon::compression::make_compressor(aethon::protocol::CompressionMode::rle);
    auto compressed = compressor->compress(input);
    auto restored = compressor->decompress(compressed, 1024);
    AETHON_REQUIRE(restored == input);

    aethon::crypto::KeyMaterial key{{0x41, 0x45, 0x54, 0x48}, 3};
    auto cipher = aethon::crypto::make_cipher(aethon::protocol::EncryptionMode::envelope);
    auto sealed = cipher->seal(restored, key);
    auto opened = cipher->open(sealed, key);
    AETHON_REQUIRE(opened == input);
}
