#include "aethon/protocol/packet_builder.hpp"

#include <utility>

namespace aethon::protocol {

PacketBuilder& PacketBuilder::version(ProtocolVersion value) {
    packet_.version = value;
    return *this;
}

PacketBuilder& PacketBuilder::kind(PacketKind value) {
    packet_.kind = value;
    return *this;
}

PacketBuilder& PacketBuilder::device(DeviceId value) {
    packet_.device = value;
    return *this;
}

PacketBuilder& PacketBuilder::timestamp(std::uint64_t value) {
    packet_.timestamp_ns = value;
    return *this;
}

PacketBuilder& PacketBuilder::sequence(std::uint32_t value) {
    packet_.sequence = value;
    return *this;
}

PacketBuilder& PacketBuilder::payload(Bytes value) {
    packet_.payload = std::move(value);
    return *this;
}

PacketBuilder& PacketBuilder::route(RoutingInfo value) {
    packet_.route = std::move(value);
    return *this;
}

PacketBuilder& PacketBuilder::fragment(FragmentInfo value) {
    packet_.fragment = value;
    return *this;
}

PacketBuilder& PacketBuilder::capabilities(DeviceCapabilities value) {
    packet_.capabilities = std::move(value);
    return *this;
}

PacketBuilder& PacketBuilder::extension(ExtensionHeader value) {
    packet_.extensions.push_back(std::move(value));
    return *this;
}

Packet PacketBuilder::build() const {
    return packet_;
}

void PacketBuilder::reset() {
    packet_ = Packet{};
}

Packet make_heartbeat(DeviceId device,
                      std::uint64_t timestamp_ns,
                      std::uint32_t sequence) {
    return PacketBuilder{}
        .kind(PacketKind::heartbeat)
        .device(device)
        .timestamp(timestamp_ns)
        .sequence(sequence)
        .build();
}

Packet make_observation(DeviceId device,
                        std::uint64_t timestamp_ns,
                        std::uint32_t sequence,
                        Bytes payload) {
    return PacketBuilder{}
        .kind(PacketKind::observation)
        .device(device)
        .timestamp(timestamp_ns)
        .sequence(sequence)
        .payload(std::move(payload))
        .build();
}

Packet make_control(DeviceId device,
                    std::uint64_t timestamp_ns,
                    std::uint32_t sequence,
                    Bytes payload,
                    RoutingInfo route) {
    return PacketBuilder{}
        .kind(PacketKind::control)
        .device(device)
        .timestamp(timestamp_ns)
        .sequence(sequence)
        .payload(std::move(payload))
        .route(std::move(route))
        .build();
}

} // namespace aethon::protocol
