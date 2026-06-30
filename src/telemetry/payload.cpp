#include "aethon/telemetry/payload.hpp"

#include "aethon/codec/binary_reader.hpp"
#include "aethon/codec/binary_writer.hpp"
#include "aethon/common/error.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace aethon::telemetry {
namespace {

constexpr std::size_t max_scalar_readings = 512;
constexpr std::size_t max_spectrum_bins = 8192;
constexpr std::size_t max_capability_bands = 128;
constexpr std::size_t max_control_body = 1024 * 1024;

ReadingQuality decode_quality(std::uint8_t value) {
    switch (value) {
    case 0:
        return ReadingQuality::good;
    case 1:
        return ReadingQuality::clipped;
    case 2:
        return ReadingQuality::interpolated;
    case 3:
        return ReadingQuality::missing_calibration;
    case 4:
        return ReadingQuality::receiver_saturated;
    default:
        throw Error(ErrorCode::malformed_packet, "unknown reading quality");
    }
}

std::uint8_t encode_quality(ReadingQuality quality) {
    return static_cast<std::uint8_t>(quality);
}

void require_count(std::size_t count, std::size_t limit, const char* label) {
    if (count > limit) {
        throw Error(ErrorCode::malformed_packet, std::string(label) + " count exceeds limit");
    }
}

void require_u16_size(std::size_t size, const char* label) {
    if (size > std::numeric_limits<std::uint16_t>::max()) {
        throw Error(ErrorCode::invalid_argument, std::string(label) + " is too large");
    }
}

std::string scalar_description(const ScalarObservation& observation) {
    std::ostringstream out;
    out << observation.readings.size()
        << " scalar readings at "
        << observation.sample_rate_hz
        << " Hz";
    return out.str();
}

std::string spectrum_description(const SpectrumFrame& frame) {
    std::ostringstream out;
    out << frame.bins.size()
        << " spectrum bins centered at "
        << frame.center_frequency_hz
        << " Hz";
    return out.str();
}

std::string control_description(const ControlEnvelope& envelope) {
    std::ostringstream out;
    out << "control command "
        << envelope.command
        << " body="
        << envelope.body.size()
        << " bytes";
    return out.str();
}

std::string capability_description(const CapabilityReport& report) {
    std::ostringstream out;
    out << "capabilities max_payload="
        << report.max_payload
        << " bands="
        << report.bands.size();
    return out.str();
}

void append_scalar_warnings(const ScalarObservation& observation, std::vector<std::string>& warnings) {
    if (observation.readings.empty()) {
        warnings.push_back("scalar observation contains no readings");
    }
    if (observation.sample_rate_hz == 0) {
        warnings.push_back("scalar observation has zero sample rate");
    }
    auto bad_quality = std::count_if(
        observation.readings.begin(),
        observation.readings.end(),
        [](const ScalarReading& reading) {
            return reading.quality != ReadingQuality::good;
        });
    if (bad_quality != 0) {
        std::ostringstream out;
        out << bad_quality << " scalar readings have degraded quality";
        warnings.push_back(out.str());
    }
}

void append_spectrum_warnings(const SpectrumFrame& frame, std::vector<std::string>& warnings) {
    if (frame.bins.empty()) {
        warnings.push_back("spectrum frame contains no bins");
    }
    if (frame.span_hz == 0) {
        warnings.push_back("spectrum frame has zero span");
    }
    if (frame.bin_width_hz == 0) {
        warnings.push_back("spectrum frame has zero bin width");
    }
    auto saturated = std::count_if(
        frame.bins.begin(),
        frame.bins.end(),
        [](const SpectrumBin& bin) {
            return bin.power_dbm_x10 >= 0 || bin.noise_dbm_x10 >= 0;
        });
    if (saturated != 0) {
        std::ostringstream out;
        out << saturated << " spectrum bins report non-negative RF power";
        warnings.push_back(out.str());
    }
}

void append_control_warnings(const ControlEnvelope& envelope, std::vector<std::string>& warnings) {
    if (envelope.command == 0) {
        warnings.push_back("control envelope has reserved command id");
    }
    if (envelope.body.empty()) {
        warnings.push_back("control envelope body is empty");
    }
}

void append_capability_warnings(const CapabilityReport& report, std::vector<std::string>& warnings) {
    if (report.max_payload == 0) {
        warnings.push_back("capability report has zero max payload");
    }
    if (report.bands.empty()) {
        warnings.push_back("capability report has no sensor bands");
    }
}

} // namespace

PayloadFormat infer_payload_format(const protocol::Packet& packet) {
    if (!packet.payload.empty()) {
        auto marker = packet.payload.front();
        if (marker >= static_cast<std::uint8_t>(PayloadFormat::scalar_observation)
            && marker <= static_cast<std::uint8_t>(PayloadFormat::capability_report)) {
            return static_cast<PayloadFormat>(marker);
        }
    }
    switch (packet.kind) {
    case protocol::PacketKind::observation:
        return PayloadFormat::scalar_observation;
    case protocol::PacketKind::spectrum:
        return PayloadFormat::spectrum_frame;
    case protocol::PacketKind::control:
        return PayloadFormat::control_envelope;
    case protocol::PacketKind::capabilities:
        return PayloadFormat::capability_report;
    case protocol::PacketKind::heartbeat:
        return PayloadFormat::unknown;
    }
    return PayloadFormat::unknown;
}

std::string payload_format_name(PayloadFormat format) {
    switch (format) {
    case PayloadFormat::unknown:
        return "unknown";
    case PayloadFormat::scalar_observation:
        return "scalar_observation";
    case PayloadFormat::spectrum_frame:
        return "spectrum_frame";
    case PayloadFormat::control_envelope:
        return "control_envelope";
    case PayloadFormat::capability_report:
        return "capability_report";
    }
    return "unknown";
}

std::string reading_quality_name(ReadingQuality quality) {
    switch (quality) {
    case ReadingQuality::good:
        return "good";
    case ReadingQuality::clipped:
        return "clipped";
    case ReadingQuality::interpolated:
        return "interpolated";
    case ReadingQuality::missing_calibration:
        return "missing_calibration";
    case ReadingQuality::receiver_saturated:
        return "receiver_saturated";
    }
    return "unknown";
}

ScalarObservation decode_scalar_observation(std::span<const std::uint8_t> payload) {
    codec::BinaryReader reader(payload);
    auto marker = reader.u8();
    if (marker != static_cast<std::uint8_t>(PayloadFormat::scalar_observation)) {
        throw Error(ErrorCode::malformed_packet, "scalar observation marker mismatch");
    }
    ScalarObservation observation;
    observation.sample_time_ns = reader.u64();
    observation.sample_rate_hz = reader.u32();
    auto count = reader.u16();
    require_count(count, max_scalar_readings, "scalar reading");
    observation.readings.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        ScalarReading reading;
        reading.channel = reader.u16();
        reading.value = static_cast<std::int32_t>(reader.u32());
        reading.scale = static_cast<std::int16_t>(reader.u16());
        reading.quality = decode_quality(reader.u8());
        observation.readings.push_back(reading);
    }
    if (!reader.empty()) {
        throw Error(ErrorCode::malformed_packet, "trailing scalar observation bytes");
    }
    return observation;
}

SpectrumFrame decode_spectrum_frame(std::span<const std::uint8_t> payload) {
    codec::BinaryReader reader(payload);
    auto marker = reader.u8();
    if (marker != static_cast<std::uint8_t>(PayloadFormat::spectrum_frame)) {
        throw Error(ErrorCode::malformed_packet, "spectrum frame marker mismatch");
    }
    SpectrumFrame frame;
    frame.center_frequency_hz = reader.u64();
    frame.span_hz = reader.u32();
    frame.bin_width_hz = reader.u16();
    auto count = reader.u16();
    require_count(count, max_spectrum_bins, "spectrum bin");
    frame.bins.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        SpectrumBin bin;
        bin.power_dbm_x10 = static_cast<std::int16_t>(reader.u16());
        bin.noise_dbm_x10 = static_cast<std::int16_t>(reader.u16());
        frame.bins.push_back(bin);
    }
    if (!reader.empty()) {
        throw Error(ErrorCode::malformed_packet, "trailing spectrum frame bytes");
    }
    return frame;
}

ControlEnvelope decode_control_envelope(std::span<const std::uint8_t> payload) {
    codec::BinaryReader reader(payload);
    auto marker = reader.u8();
    if (marker != static_cast<std::uint8_t>(PayloadFormat::control_envelope)) {
        throw Error(ErrorCode::malformed_packet, "control envelope marker mismatch");
    }
    ControlEnvelope envelope;
    envelope.command = reader.u16();
    envelope.flags = reader.u16();
    envelope.correlation_id = reader.u32();
    auto body_size = reader.u32();
    require_count(body_size, max_control_body, "control body");
    envelope.body = reader.bytes(body_size);
    if (!reader.empty()) {
        throw Error(ErrorCode::malformed_packet, "trailing control envelope bytes");
    }
    return envelope;
}

CapabilityReport decode_capability_report(std::span<const std::uint8_t> payload) {
    codec::BinaryReader reader(payload);
    auto marker = reader.u8();
    if (marker != static_cast<std::uint8_t>(PayloadFormat::capability_report)) {
        throw Error(ErrorCode::malformed_packet, "capability report marker mismatch");
    }
    CapabilityReport report;
    report.max_payload = reader.u16();
    auto flags = reader.u8();
    report.compression = (flags & 0x01) != 0;
    report.encryption = (flags & 0x02) != 0;
    report.gps_time = (flags & 0x04) != 0;
    auto band_count = reader.u16();
    require_count(band_count, max_capability_bands, "capability band");
    report.bands.reserve(band_count);
    for (std::uint16_t i = 0; i < band_count; ++i) {
        report.bands.push_back(reader.string(128));
    }
    if (!reader.empty()) {
        throw Error(ErrorCode::malformed_packet, "trailing capability report bytes");
    }
    return report;
}

Bytes encode_scalar_observation(const ScalarObservation& observation) {
    require_u16_size(observation.readings.size(), "scalar reading list");
    require_count(observation.readings.size(), max_scalar_readings, "scalar reading");
    codec::BinaryWriter writer;
    writer.u8(static_cast<std::uint8_t>(PayloadFormat::scalar_observation));
    writer.u64(observation.sample_time_ns);
    writer.u32(observation.sample_rate_hz);
    writer.u16(static_cast<std::uint16_t>(observation.readings.size()));
    for (const auto& reading : observation.readings) {
        writer.u16(reading.channel);
        writer.u32(static_cast<std::uint32_t>(reading.value));
        writer.u16(static_cast<std::uint16_t>(reading.scale));
        writer.u8(encode_quality(reading.quality));
    }
    return writer.take();
}

Bytes encode_spectrum_frame(const SpectrumFrame& frame) {
    require_u16_size(frame.bins.size(), "spectrum bin list");
    require_count(frame.bins.size(), max_spectrum_bins, "spectrum bin");
    codec::BinaryWriter writer;
    writer.u8(static_cast<std::uint8_t>(PayloadFormat::spectrum_frame));
    writer.u64(frame.center_frequency_hz);
    writer.u32(frame.span_hz);
    writer.u16(frame.bin_width_hz);
    writer.u16(static_cast<std::uint16_t>(frame.bins.size()));
    for (const auto& bin : frame.bins) {
        writer.u16(static_cast<std::uint16_t>(bin.power_dbm_x10));
        writer.u16(static_cast<std::uint16_t>(bin.noise_dbm_x10));
    }
    return writer.take();
}

Bytes encode_control_envelope(const ControlEnvelope& envelope) {
    require_count(envelope.body.size(), max_control_body, "control body");
    codec::BinaryWriter writer;
    writer.u8(static_cast<std::uint8_t>(PayloadFormat::control_envelope));
    writer.u16(envelope.command);
    writer.u16(envelope.flags);
    writer.u32(envelope.correlation_id);
    writer.u32(static_cast<std::uint32_t>(envelope.body.size()));
    writer.bytes(envelope.body);
    return writer.take();
}

Bytes encode_capability_report(const CapabilityReport& report) {
    require_u16_size(report.bands.size(), "capability band list");
    require_count(report.bands.size(), max_capability_bands, "capability band");
    codec::BinaryWriter writer;
    writer.u8(static_cast<std::uint8_t>(PayloadFormat::capability_report));
    writer.u16(report.max_payload);
    std::uint8_t flags = 0;
    if (report.compression) {
        flags |= 0x01;
    }
    if (report.encryption) {
        flags |= 0x02;
    }
    if (report.gps_time) {
        flags |= 0x04;
    }
    writer.u8(flags);
    writer.u16(static_cast<std::uint16_t>(report.bands.size()));
    for (const auto& band : report.bands) {
        writer.string(band);
    }
    return writer.take();
}

DecodedPayload decode_packet_payload(const protocol::Packet& packet) {
    DecodedPayload decoded;
    decoded.format = infer_payload_format(packet);
    if (packet.payload.empty()) {
        decoded.warnings.push_back("packet payload is empty");
        return decoded;
    }
    try {
        switch (decoded.format) {
        case PayloadFormat::scalar_observation:
            decoded.scalar = decode_scalar_observation(packet.payload);
            append_scalar_warnings(*decoded.scalar, decoded.warnings);
            break;
        case PayloadFormat::spectrum_frame:
            decoded.spectrum = decode_spectrum_frame(packet.payload);
            append_spectrum_warnings(*decoded.spectrum, decoded.warnings);
            break;
        case PayloadFormat::control_envelope:
            decoded.control = decode_control_envelope(packet.payload);
            append_control_warnings(*decoded.control, decoded.warnings);
            break;
        case PayloadFormat::capability_report:
            decoded.capabilities = decode_capability_report(packet.payload);
            append_capability_warnings(*decoded.capabilities, decoded.warnings);
            break;
        case PayloadFormat::unknown:
            decoded.warnings.push_back("unknown payload format");
            break;
        }
    } catch (const Error& error) {
        decoded.warnings.push_back(error.what());
    }
    return decoded;
}

PayloadSummary summarize_payload(const protocol::Packet& packet) {
    auto decoded = decode_packet_payload(packet);
    PayloadSummary summary;
    summary.format = decoded.format;
    summary.payload_bytes = packet.payload.size();
    summary.warnings = decoded.warnings;
    if (decoded.scalar) {
        summary.logical_items = decoded.scalar->readings.size();
        summary.description = scalar_description(*decoded.scalar);
    } else if (decoded.spectrum) {
        summary.logical_items = decoded.spectrum->bins.size();
        summary.description = spectrum_description(*decoded.spectrum);
    } else if (decoded.control) {
        summary.logical_items = decoded.control->body.size();
        summary.description = control_description(*decoded.control);
    } else if (decoded.capabilities) {
        summary.logical_items = decoded.capabilities->bands.size();
        summary.description = capability_description(*decoded.capabilities);
    } else {
        summary.description = "unparsed payload";
    }
    return summary;
}

std::string render_payload_summary(const PayloadSummary& summary) {
    std::ostringstream out;
    out << "payload "
        << payload_format_name(summary.format)
        << "\n"
        << "  bytes: "
        << summary.payload_bytes
        << "\n"
        << "  logical_items: "
        << summary.logical_items
        << "\n"
        << "  description: "
        << summary.description
        << "\n";
    for (const auto& warning : summary.warnings) {
        out << "  warning: "
            << warning
            << "\n";
    }
    return out.str();
}

} // namespace aethon::telemetry
