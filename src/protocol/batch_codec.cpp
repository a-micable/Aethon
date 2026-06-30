#include "aethon/protocol/batch_codec.hpp"

#include "aethon/codec/binary_reader.hpp"
#include "aethon/codec/binary_writer.hpp"
#include "aethon/common/error.hpp"
#include "aethon/protocol/packet.hpp"

namespace aethon::protocol {

Bytes encode_packet_batch(const PacketBatch& batch) {
    codec::BinaryWriter writer;
    if (batch.packets.size() > 65535) {
        throw Error(ErrorCode::invalid_argument, "packet batch is too large");
    }
    writer.u16(static_cast<std::uint16_t>(batch.packets.size()));
    for (const auto& packet : batch.packets) {
        auto frame = encode_packet(packet);
        writer.u32(static_cast<std::uint32_t>(frame.size()));
        writer.bytes(frame);
    }
    return writer.take();
}

PacketBatch decode_packet_batch(std::span<const std::uint8_t> bytes) {
    codec::BinaryReader reader(bytes);
    PacketBatch batch;
    auto count = reader.u16();
    batch.packets.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        auto frame_size = reader.u32();
        auto frame = reader.bytes(frame_size);
        batch.packets.push_back(decode_packet(frame));
    }
    if (!reader.empty()) {
        throw Error(ErrorCode::malformed_packet, "trailing packet batch bytes");
    }
    return batch;
}

} // namespace aethon::protocol
