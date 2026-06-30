#pragma once

#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::protocol {

enum class NegotiationDecision {
    accepted,
    downgraded_version,
    disabled_compression,
    disabled_encryption,
    rejected_payload_limit,
    rejected_version,
};

struct NegotiationPolicy {
    ProtocolVersion minimum_version = ProtocolVersion::v1;
    ProtocolVersion maximum_version = ProtocolVersion::v3;
    bool require_encryption = false;
    bool prefer_compression = true;
    std::uint16_t minimum_payload = 256;
};

struct NegotiationNote {
    NegotiationDecision decision = NegotiationDecision::accepted;
    std::string message;
};

struct NegotiationResult {
    bool accepted = false;
    NegotiatedProtocol protocol;
    std::uint16_t remote_max_payload = 0;
    std::vector<NegotiationNote> notes;
};

[[nodiscard]] NegotiationResult negotiate_protocol(const NegotiationPolicy& policy,
                                                   const DeviceCapabilities& remote);

[[nodiscard]] bool version_allowed(ProtocolVersion version, const NegotiationPolicy& policy);
[[nodiscard]] ProtocolVersion clamp_version(ProtocolVersion requested, const NegotiationPolicy& policy);
[[nodiscard]] std::string negotiation_decision_name(NegotiationDecision decision);
[[nodiscard]] std::string render_negotiation_result(const NegotiationResult& result);

} // namespace aethon::protocol
