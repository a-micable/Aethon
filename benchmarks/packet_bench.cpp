#include "aethon/protocol/packet.hpp"
#include <chrono>
#include <iostream>

int main() {
    aethon::protocol::Packet p;
    p.device = 9001;
    p.kind = aethon::protocol::PacketKind::spectrum;
    p.route = aethon::protocol::RoutingInfo{2, 44, 5, {10, 11, 12}};
    p.payload.resize(2048, 0x5a);

    constexpr int iterations = 10000;
    auto start = std::chrono::steady_clock::now();
    std::size_t bytes = 0;
    for (int i = 0; i < iterations; ++i) {
        p.sequence = static_cast<std::uint32_t>(i);
        auto frame = aethon::protocol::encode_packet(p);
        bytes += aethon::protocol::decode_packet(frame).payload.size();
    }

    auto end = std::chrono::steady_clock::now();
    std::cout << "iterations=" << iterations
              << " payload_bytes=" << bytes
              << " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << '\n';
}
