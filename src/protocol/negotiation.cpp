#include "aethon/protocol/negotiation.hpp"

#include "aethon/protocol/inspector.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::protocol {
namespace {

std::uint8_t version_number(ProtocolVersion version) {
    return static_cast<std::uint8_t>(version);
}

void add_note(NegotiationResult& result, NegotiationDecision decision, std::string message) {
    result.notes.push_back(NegotiationNote{
        decision,
        std::move(message),
    });
}

ProtocolVersion highest_common_version(const NegotiationPolicy& policy) {
    if (version_number(policy.maximum_version) < version_number(policy.minimum_version)) {
        return policy.minimum_version;
    }
    return policy.maximum_version;
}

} // namespace

NegotiationResult negotiate_protocol(const NegotiationPolicy& policy,
                                     const DeviceCapabilities& remote) {
    NegotiationResult result;
    result.remote_max_payload = remote.max_payload;
    result.protocol.selected = highest_common_version(policy);

    if (version_number(policy.maximum_version) < version_number(policy.minimum_version)) {
        add_note(result, NegotiationDecision::rejected_version, "invalid local version policy");
        result.accepted = false;
        return result;
    }

    if (remote.max_payload < policy.minimum_payload) {
        std::ostringstream message;
        message << "remote max payload "
                << remote.max_payload
                << " is below required "
                << policy.minimum_payload;
        add_note(result, NegotiationDecision::rejected_payload_limit, message.str());
        result.accepted = false;
        return result;
    }

    if (policy.require_encryption && !remote.supports_encryption) {
        add_note(result, NegotiationDecision::disabled_encryption, "remote does not support encryption");
        result.accepted = false;
        return result;
    }

    result.protocol.compression = policy.prefer_compression && remote.supports_compression;
    result.protocol.encryption = remote.supports_encryption;

    if (policy.prefer_compression && !remote.supports_compression) {
        add_note(result, NegotiationDecision::disabled_compression, "compression disabled by remote capabilities");
    }
    if (!remote.supports_encryption) {
        add_note(result, NegotiationDecision::disabled_encryption, "encryption disabled by remote capabilities");
    }
    if (result.notes.empty()) {
        add_note(result, NegotiationDecision::accepted, "negotiation accepted without downgrade");
    }

    result.accepted = true;
    return result;
}

bool version_allowed(ProtocolVersion version, const NegotiationPolicy& policy) {
    auto v = version_number(version);
    return v >= version_number(policy.minimum_version)
        && v <= version_number(policy.maximum_version);
}

ProtocolVersion clamp_version(ProtocolVersion requested, const NegotiationPolicy& policy) {
    auto requested_number = version_number(requested);
    auto min_number = version_number(policy.minimum_version);
    auto max_number = version_number(policy.maximum_version);
    auto clamped = std::clamp(requested_number, min_number, max_number);
    return static_cast<ProtocolVersion>(clamped);
}

std::string negotiation_decision_name(NegotiationDecision decision) {
    switch (decision) {
    case NegotiationDecision::accepted:
        return "accepted";
    case NegotiationDecision::downgraded_version:
        return "downgraded_version";
    case NegotiationDecision::disabled_compression:
        return "disabled_compression";
    case NegotiationDecision::disabled_encryption:
        return "disabled_encryption";
    case NegotiationDecision::rejected_payload_limit:
        return "rejected_payload_limit";
    case NegotiationDecision::rejected_version:
        return "rejected_version";
    }
    return "unknown";
}

std::string render_negotiation_result(const NegotiationResult& result) {
    std::ostringstream out;
    out << "negotiation "
        << (result.accepted ? "accepted" : "rejected")
        << "\n"
        << "  version: "
        << version_name(result.protocol.selected)
        << "\n"
        << "  compression: "
        << (result.protocol.compression ? "yes" : "no")
        << "\n"
        << "  encryption: "
        << (result.protocol.encryption ? "yes" : "no")
        << "\n"
        << "  remote_max_payload: "
        << result.remote_max_payload
        << "\n";
    for (const auto& note : result.notes) {
        out << "  note: "
            << negotiation_decision_name(note.decision)
            << " "
            << note.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::protocol
