#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::protocol {

class PacketBuilder {
public:
    PacketBuilder& version(ProtocolVersion value);
    PacketBuilder& kind(PacketKind value);
    PacketBuilder& device(DeviceId value);
    PacketBuilder& timestamp(std::uint64_t value);
    PacketBuilder& sequence(std::uint32_t value);
    PacketBuilder& payload(Bytes value);
    PacketBuilder& route(RoutingInfo value);
    PacketBuilder& fragment(FragmentInfo value);
    PacketBuilder& capabilities(DeviceCapabilities value);
    PacketBuilder& extension(ExtensionHeader value);

    [[nodiscard]] Packet build() const;
    void reset();

private:
    Packet packet_;
};

[[nodiscard]] Packet make_heartbeat(DeviceId device,
                                    std::uint64_t timestamp_ns,
                                    std::uint32_t sequence);
[[nodiscard]] Packet make_observation(DeviceId device,
                                      std::uint64_t timestamp_ns,
                                      std::uint32_t sequence,
                                      Bytes payload);
[[nodiscard]] Packet make_control(DeviceId device,
                                  std::uint64_t timestamp_ns,
                                  std::uint32_t sequence,
                                  Bytes payload,
                                  RoutingInfo route);

} // namespace aethon::protocol
