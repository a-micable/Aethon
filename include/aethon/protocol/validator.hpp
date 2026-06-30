#pragma once

#include "aethon/protocol/extension_registry.hpp"
#include "aethon/protocol/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aethon::protocol {

enum class ValidationSeverity {
    note,
    warning,
    error,
};

struct PacketValidationRuleSet {
    std::uint64_t max_timestamp_ns = 0;
    std::uint32_t max_sequence_jump = 1000000;
    std::size_t max_extensions = 64;
    std::size_t max_payload_size = 16 * 1024 * 1024;
    bool require_route_for_control = true;
};

struct PacketValidationIssue {
    ValidationSeverity severity = ValidationSeverity::note;
    std::string code;
    std::string message;
};

struct PacketValidationReport {
    std::vector<PacketValidationIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] PacketValidationReport validate_packet_semantics(
    const Packet& packet,
    const PacketValidationRuleSet& rules = {},
    const ExtensionRegistry& registry = default_extension_registry());

[[nodiscard]] std::string validation_severity_name(ValidationSeverity severity);
[[nodiscard]] std::string render_packet_validation_report(const PacketValidationReport& report);

} // namespace aethon::protocol
