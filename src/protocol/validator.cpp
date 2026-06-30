#include "aethon/protocol/validator.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::protocol {
namespace {

void add_issue(PacketValidationReport& report,
               ValidationSeverity severity,
               std::string code,
               std::string message) {
    report.issues.push_back(PacketValidationIssue{
        severity,
        std::move(code),
        std::move(message),
    });
}

ValidationSeverity warning() {
    return ValidationSeverity::warning;
}

void validate_payload(const Packet& packet,
                      const PacketValidationRuleSet& rules,
                      PacketValidationReport& report) {
    if (packet.payload.size() > rules.max_payload_size) {
        add_issue(
            report,
            ValidationSeverity::error,
            "packet.payload_too_large",
            "packet payload exceeds configured maximum");
    }
    if (packet.payload.empty() && packet.kind != PacketKind::heartbeat) {
        add_issue(
            report,
            warning(),
            "packet.empty_payload",
            "non-heartbeat packet has an empty payload");
    }
}

void validate_fragment(const Packet& packet, PacketValidationReport& report) {
    if (!packet.fragment) {
        return;
    }
    if (packet.fragment->count == 0) {
        add_issue(
            report,
            ValidationSeverity::error,
            "fragment.zero_count",
            "fragment count is zero");
    }
    if (packet.fragment->sequence >= packet.fragment->count) {
        add_issue(
            report,
            ValidationSeverity::error,
            "fragment.sequence_out_of_range",
            "fragment sequence is outside fragment count");
    }
}

void validate_route(const Packet& packet,
                    const PacketValidationRuleSet& rules,
                    PacketValidationReport& report) {
    if (packet.kind == PacketKind::control && rules.require_route_for_control && !packet.route) {
        add_issue(
            report,
            warning(),
            "route.missing_control_route",
            "control packet has no route information");
    }
    if (!packet.route) {
        return;
    }
    if (packet.route->relay_path.size() > 16) {
        add_issue(
            report,
            warning(),
            "route.long_relay_path",
            "relay path contains more than sixteen hops");
    }
}

void validate_capabilities(const Packet& packet, PacketValidationReport& report) {
    if (!packet.capabilities) {
        return;
    }
    if (packet.capabilities->max_payload == 0) {
        add_issue(
            report,
            ValidationSeverity::error,
            "capabilities.zero_payload",
            "device capabilities report a zero max payload");
    }
    if (packet.capabilities->sensor_bands.empty()) {
        add_issue(
            report,
            warning(),
            "capabilities.no_bands",
            "device capabilities contain no sensor bands");
    }
}

void validate_extensions_for_packet(const Packet& packet,
                                    const PacketValidationRuleSet& rules,
                                    const ExtensionRegistry& registry,
                                    PacketValidationReport& report) {
    if (packet.extensions.size() > rules.max_extensions) {
        add_issue(
            report,
            ValidationSeverity::error,
            "extension.too_many",
            "packet has more extensions than allowed");
    }
    auto issues = validate_extensions(packet.extensions, registry);
    for (const auto& issue : issues) {
        add_issue(
            report,
            warning(),
            "extension.invalid",
            issue.message);
    }
}

} // namespace

bool PacketValidationReport::ok() const noexcept {
    return std::none_of(
        issues.begin(),
        issues.end(),
        [](const PacketValidationIssue& issue) {
            return issue.severity == ValidationSeverity::error;
        });
}

PacketValidationReport validate_packet_semantics(const Packet& packet,
                                                 const PacketValidationRuleSet& rules,
                                                 const ExtensionRegistry& registry) {
    PacketValidationReport report;
    if (rules.max_timestamp_ns != 0 && packet.timestamp_ns > rules.max_timestamp_ns) {
        add_issue(
            report,
            warning(),
            "packet.timestamp_future",
            "packet timestamp is greater than configured maximum");
    }
    validate_payload(packet, rules, report);
    validate_fragment(packet, report);
    validate_route(packet, rules, report);
    validate_capabilities(packet, report);
    validate_extensions_for_packet(packet, rules, registry, report);

    if (report.issues.empty()) {
        add_issue(
            report,
            ValidationSeverity::note,
            "packet.ok",
            "packet semantic checks passed");
    }
    return report;
}

std::string validation_severity_name(ValidationSeverity severity) {
    switch (severity) {
    case ValidationSeverity::note:
        return "note";
    case ValidationSeverity::warning:
        return "warning";
    case ValidationSeverity::error:
        return "error";
    }
    return "unknown";
}

std::string render_packet_validation_report(const PacketValidationReport& report) {
    std::ostringstream out;
    out << "packet_validation\n"
        << "  ok: "
        << (report.ok() ? "yes" : "no")
        << "\n";
    for (const auto& issue : report.issues) {
        out << "  issue: "
            << validation_severity_name(issue.severity)
            << " "
            << issue.code
            << " "
            << issue.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::protocol
