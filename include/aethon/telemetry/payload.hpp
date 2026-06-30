#pragma once

#include "aethon/common/bytes.hpp"
#include "aethon/protocol/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace aethon::telemetry {

enum class PayloadFormat : std::uint8_t {
    unknown = 0,
    scalar_observation = 1,
    spectrum_frame = 2,
    control_envelope = 3,
    capability_report = 4,
};

enum class ReadingQuality : std::uint8_t {
    good = 0,
    clipped = 1,
    interpolated = 2,
    missing_calibration = 3,
    receiver_saturated = 4,
};

struct ScalarReading {
    std::uint16_t channel = 0;
    std::int32_t value = 0;
    std::int16_t scale = 0;
    ReadingQuality quality = ReadingQuality::good;
};

struct ScalarObservation {
    std::uint64_t sample_time_ns = 0;
    std::uint32_t sample_rate_hz = 0;
    std::vector<ScalarReading> readings;
};

struct SpectrumBin {
    std::int16_t power_dbm_x10 = 0;
    std::int16_t noise_dbm_x10 = 0;
};

struct SpectrumFrame {
    std::uint64_t center_frequency_hz = 0;
    std::uint32_t span_hz = 0;
    std::uint16_t bin_width_hz = 0;
    std::vector<SpectrumBin> bins;
};

struct ControlEnvelope {
    std::uint16_t command = 0;
    std::uint16_t flags = 0;
    std::uint32_t correlation_id = 0;
    Bytes body;
};

struct CapabilityReport {
    std::uint16_t max_payload = 0;
    bool compression = false;
    bool encryption = false;
    bool gps_time = false;
    std::vector<std::string> bands;
};

struct DecodedPayload {
    PayloadFormat format = PayloadFormat::unknown;
    std::optional<ScalarObservation> scalar;
    std::optional<SpectrumFrame> spectrum;
    std::optional<ControlEnvelope> control;
    std::optional<CapabilityReport> capabilities;
    std::vector<std::string> warnings;
};

struct PayloadSummary {
    PayloadFormat format = PayloadFormat::unknown;
    std::size_t logical_items = 0;
    std::size_t payload_bytes = 0;
    std::string description;
    std::vector<std::string> warnings;
};

[[nodiscard]] PayloadFormat infer_payload_format(const protocol::Packet& packet);
[[nodiscard]] std::string payload_format_name(PayloadFormat format);
[[nodiscard]] std::string reading_quality_name(ReadingQuality quality);

[[nodiscard]] ScalarObservation decode_scalar_observation(std::span<const std::uint8_t> payload);
[[nodiscard]] SpectrumFrame decode_spectrum_frame(std::span<const std::uint8_t> payload);
[[nodiscard]] ControlEnvelope decode_control_envelope(std::span<const std::uint8_t> payload);
[[nodiscard]] CapabilityReport decode_capability_report(std::span<const std::uint8_t> payload);

[[nodiscard]] Bytes encode_scalar_observation(const ScalarObservation& observation);
[[nodiscard]] Bytes encode_spectrum_frame(const SpectrumFrame& frame);
[[nodiscard]] Bytes encode_control_envelope(const ControlEnvelope& envelope);
[[nodiscard]] Bytes encode_capability_report(const CapabilityReport& report);

[[nodiscard]] DecodedPayload decode_packet_payload(const protocol::Packet& packet);
[[nodiscard]] PayloadSummary summarize_payload(const protocol::Packet& packet);
[[nodiscard]] std::string render_payload_summary(const PayloadSummary& summary);

} // namespace aethon::telemetry
