#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/runtime/ingest_queue.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::control {

enum class PressureLevel : std::uint8_t {
    nominal,
    watch,
    throttle,
    shed,
    stop,
};

enum class AdmissionAction : std::uint8_t {
    accept,
    delay,
    downsample,
    shed,
    reject,
};

struct BackpressureThresholds {
    double watch_ratio = 0.50;
    double throttle_ratio = 0.70;
    double shed_ratio = 0.85;
    double stop_ratio = 0.97;
    std::uint64_t stale_sample_ns = 1'000'000'000;
};

struct BackpressureConfig {
    std::size_t queue_capacity_packets = 1024;
    std::size_t payload_capacity_bytes = 16 * 1024 * 1024;
    std::uint64_t min_delay_ns = 50'000;
    std::uint64_t max_delay_ns = 50'000'000;
    std::uint32_t max_downsample_factor = 16;
    bool protect_control_packets = true;
    BackpressureThresholds thresholds;
};

struct PressureSample {
    std::uint64_t time_ns = 0;
    std::size_t queued_packets = 0;
    std::size_t queued_payload_bytes = 0;
    std::size_t inflight_batches = 0;
    std::size_t stalled_devices = 0;
};

struct DevicePressureState {
    protocol::DeviceId device = 0;
    std::uint64_t last_seen_ns = 0;
    std::uint64_t admitted_packets = 0;
    std::uint64_t delayed_packets = 0;
    std::uint64_t downsampled_packets = 0;
    std::uint64_t shed_packets = 0;
    std::uint64_t rejected_packets = 0;
    std::uint32_t current_downsample_factor = 1;
    std::uint64_t next_admit_time_ns = 0;
    PressureLevel last_level = PressureLevel::nominal;
};

struct AdmissionDecision {
    AdmissionAction action = AdmissionAction::accept;
    PressureLevel pressure = PressureLevel::nominal;
    std::uint64_t delay_ns = 0;
    std::uint32_t downsample_factor = 1;
    std::string reason;
};

struct BackpressureSnapshot {
    PressureSample sample;
    PressureLevel level = PressureLevel::nominal;
    double packet_ratio = 0.0;
    double payload_ratio = 0.0;
    std::uint64_t decisions = 0;
    std::vector<DevicePressureState> devices;
};

class BackpressureController {
public:
    explicit BackpressureController(BackpressureConfig config = {});

    [[nodiscard]] const BackpressureConfig& config() const noexcept;
    [[nodiscard]] PressureLevel level() const noexcept;
    [[nodiscard]] PressureSample sample() const noexcept;
    [[nodiscard]] BackpressureSnapshot snapshot() const;

    void observe_queue(const runtime::IngestQueueStats& stats, std::uint64_t time_ns);
    void observe_sample(PressureSample sample);
    [[nodiscard]] AdmissionDecision decide(const protocol::Packet& packet, std::uint64_t time_ns);
    void note_batch_completed(std::size_t packet_count, std::size_t payload_bytes, std::uint64_t time_ns);
    void reset_device(protocol::DeviceId device);
    void reset();

private:
    [[nodiscard]] PressureLevel classify(const PressureSample& sample) const;
    [[nodiscard]] double packet_ratio(const PressureSample& sample) const noexcept;
    [[nodiscard]] double payload_ratio(const PressureSample& sample) const noexcept;
    [[nodiscard]] AdmissionDecision make_decision(const protocol::Packet& packet,
                                                  DevicePressureState& device,
                                                  std::uint64_t time_ns);
    [[nodiscard]] bool is_protected(const protocol::Packet& packet) const noexcept;
    [[nodiscard]] std::uint64_t delay_for(PressureLevel level, const DevicePressureState& device) const noexcept;
    [[nodiscard]] std::uint32_t downsample_for(PressureLevel level, const DevicePressureState& device) const noexcept;
    void apply_decision(DevicePressureState& device, const AdmissionDecision& decision, std::uint64_t time_ns);

    BackpressureConfig config_;
    PressureSample sample_;
    PressureLevel level_ = PressureLevel::nominal;
    std::map<protocol::DeviceId, DevicePressureState> devices_;
    std::uint64_t decisions_ = 0;
};

[[nodiscard]] PressureLevel classify_pressure(const PressureSample& sample, const BackpressureConfig& config);
[[nodiscard]] double pressure_ratio(std::size_t used, std::size_t capacity) noexcept;
[[nodiscard]] std::string pressure_level_name(PressureLevel level);
[[nodiscard]] std::string admission_action_name(AdmissionAction action);
[[nodiscard]] std::string render_admission_decision(const AdmissionDecision& decision);
[[nodiscard]] std::string render_backpressure_snapshot(const BackpressureSnapshot& snapshot);

} // namespace aethon::control
