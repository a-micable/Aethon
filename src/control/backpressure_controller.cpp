#include "aethon/control/backpressure_controller.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <utility>

namespace aethon::control {
namespace {

std::uint8_t level_rank(PressureLevel level) {
    return static_cast<std::uint8_t>(level);
}

PressureLevel max_level(PressureLevel lhs, PressureLevel rhs) {
    return level_rank(lhs) >= level_rank(rhs) ? lhs : rhs;
}

std::uint64_t clamp_delay(std::uint64_t delay,
                          std::uint64_t min_delay,
                          std::uint64_t max_delay) {
    if (delay < min_delay) {
        return min_delay;
    }
    if (delay > max_delay) {
        return max_delay;
    }
    return delay;
}

std::string device_reason(const protocol::Packet& packet, const char* suffix) {
    std::ostringstream out;
    out << "device "
        << packet.device
        << ' '
        << suffix;
    return out.str();
}

} // namespace

BackpressureController::BackpressureController(BackpressureConfig config)
    : config_(config) {}

const BackpressureConfig& BackpressureController::config() const noexcept {
    return config_;
}

PressureLevel BackpressureController::level() const noexcept {
    return level_;
}

PressureSample BackpressureController::sample() const noexcept {
    return sample_;
}

BackpressureSnapshot BackpressureController::snapshot() const {
    BackpressureSnapshot snapshot;
    snapshot.sample = sample_;
    snapshot.level = level_;
    snapshot.packet_ratio = packet_ratio(sample_);
    snapshot.payload_ratio = payload_ratio(sample_);
    snapshot.decisions = decisions_;
    snapshot.devices.reserve(devices_.size());
    for (const auto& [unused, state] : devices_) {
        (void)unused;
        snapshot.devices.push_back(state);
    }
    return snapshot;
}

void BackpressureController::observe_queue(const runtime::IngestQueueStats& stats, std::uint64_t time_ns) {
    PressureSample sample;
    sample.time_ns = time_ns;
    sample.queued_packets = stats.queued_packets;
    sample.queued_payload_bytes = stats.queued_payload_bytes;
    observe_sample(sample);
}

void BackpressureController::observe_sample(PressureSample sample) {
    sample_ = sample;
    level_ = classify(sample_);
}

AdmissionDecision BackpressureController::decide(const protocol::Packet& packet, std::uint64_t time_ns) {
    auto& device = devices_[packet.device];
    if (device.device == 0 && packet.device != 0) {
        device.device = packet.device;
    }
    if (device.device == 0) {
        device.device = packet.device;
    }
    device.last_seen_ns = time_ns;
    device.last_level = level_;
    auto decision = make_decision(packet, device, time_ns);
    apply_decision(device, decision, time_ns);
    ++decisions_;
    return decision;
}

void BackpressureController::note_batch_completed(std::size_t packet_count,
                                                  std::size_t payload_bytes,
                                                  std::uint64_t time_ns) {
    if (sample_.queued_packets >= packet_count) {
        sample_.queued_packets -= packet_count;
    } else {
        sample_.queued_packets = 0;
    }
    if (sample_.queued_payload_bytes >= payload_bytes) {
        sample_.queued_payload_bytes -= payload_bytes;
    } else {
        sample_.queued_payload_bytes = 0;
    }
    sample_.time_ns = time_ns;
    level_ = classify(sample_);
}

void BackpressureController::reset_device(protocol::DeviceId device) {
    devices_.erase(device);
}

void BackpressureController::reset() {
    sample_ = {};
    level_ = PressureLevel::nominal;
    devices_.clear();
    decisions_ = 0;
}

PressureLevel BackpressureController::classify(const PressureSample& sample) const {
    return classify_pressure(sample, config_);
}

double BackpressureController::packet_ratio(const PressureSample& sample) const noexcept {
    return pressure_ratio(sample.queued_packets, config_.queue_capacity_packets);
}

double BackpressureController::payload_ratio(const PressureSample& sample) const noexcept {
    return pressure_ratio(sample.queued_payload_bytes, config_.payload_capacity_bytes);
}

AdmissionDecision BackpressureController::make_decision(const protocol::Packet& packet,
                                                        DevicePressureState& device,
                                                        std::uint64_t time_ns) {
    AdmissionDecision decision;
    decision.pressure = level_;
    decision.downsample_factor = device.current_downsample_factor;

    if (is_protected(packet) && level_ != PressureLevel::stop) {
        decision.action = AdmissionAction::accept;
        decision.downsample_factor = 1;
        decision.reason = device_reason(packet, "accepted as protected control traffic");
        return decision;
    }

    if (time_ns < device.next_admit_time_ns) {
        decision.action = AdmissionAction::delay;
        decision.delay_ns = device.next_admit_time_ns - time_ns;
        decision.reason = device_reason(packet, "is still inside a prior delay window");
        return decision;
    }

    switch (level_) {
    case PressureLevel::nominal:
        decision.action = AdmissionAction::accept;
        decision.downsample_factor = 1;
        decision.reason = device_reason(packet, "accepted under nominal pressure");
        break;
    case PressureLevel::watch:
        decision.action = AdmissionAction::accept;
        decision.downsample_factor = 1;
        decision.reason = device_reason(packet, "accepted while pressure is watched");
        break;
    case PressureLevel::throttle:
        decision.action = AdmissionAction::delay;
        decision.delay_ns = delay_for(level_, device);
        decision.reason = device_reason(packet, "delayed by throttle pressure");
        break;
    case PressureLevel::shed:
        decision.action = AdmissionAction::downsample;
        decision.downsample_factor = downsample_for(level_, device);
        decision.reason = device_reason(packet, "downsampled by shed pressure");
        break;
    case PressureLevel::stop:
        decision.action = AdmissionAction::reject;
        decision.downsample_factor = config_.max_downsample_factor;
        decision.reason = device_reason(packet, "rejected by stop pressure");
        break;
    }
    return decision;
}

bool BackpressureController::is_protected(const protocol::Packet& packet) const noexcept {
    if (!config_.protect_control_packets) {
        return false;
    }
    return packet.kind == protocol::PacketKind::control
        || packet.kind == protocol::PacketKind::heartbeat
        || packet.kind == protocol::PacketKind::capabilities;
}

std::uint64_t BackpressureController::delay_for(PressureLevel level,
                                                const DevicePressureState& device) const noexcept {
    const auto repeats = std::min<std::uint64_t>(device.delayed_packets + 1, 32);
    std::uint64_t base = config_.min_delay_ns;
    switch (level) {
    case PressureLevel::nominal:
    case PressureLevel::watch:
        base = 0;
        break;
    case PressureLevel::throttle:
        base = config_.min_delay_ns * 2;
        break;
    case PressureLevel::shed:
        base = config_.min_delay_ns * 8;
        break;
    case PressureLevel::stop:
        base = config_.max_delay_ns;
        break;
    }
    if (base == 0) {
        return 0;
    }
    return clamp_delay(base * repeats, config_.min_delay_ns, config_.max_delay_ns);
}

std::uint32_t BackpressureController::downsample_for(PressureLevel level,
                                                     const DevicePressureState& device) const noexcept {
    std::uint32_t factor = 1;
    switch (level) {
    case PressureLevel::nominal:
    case PressureLevel::watch:
    case PressureLevel::throttle:
        factor = 1;
        break;
    case PressureLevel::shed:
        factor = std::max<std::uint32_t>(2, device.current_downsample_factor * 2);
        break;
    case PressureLevel::stop:
        factor = config_.max_downsample_factor;
        break;
    }
    return std::min(factor, config_.max_downsample_factor);
}

void BackpressureController::apply_decision(DevicePressureState& device,
                                            const AdmissionDecision& decision,
                                            std::uint64_t time_ns) {
    device.last_level = decision.pressure;
    switch (decision.action) {
    case AdmissionAction::accept:
        ++device.admitted_packets;
        device.current_downsample_factor = 1;
        device.next_admit_time_ns = time_ns;
        break;
    case AdmissionAction::delay:
        ++device.delayed_packets;
        device.next_admit_time_ns = time_ns + decision.delay_ns;
        break;
    case AdmissionAction::downsample:
        ++device.downsampled_packets;
        device.current_downsample_factor = std::max<std::uint32_t>(1, decision.downsample_factor);
        device.next_admit_time_ns = time_ns;
        break;
    case AdmissionAction::shed:
        ++device.shed_packets;
        device.next_admit_time_ns = time_ns;
        break;
    case AdmissionAction::reject:
        ++device.rejected_packets;
        device.current_downsample_factor = std::max<std::uint32_t>(1, decision.downsample_factor);
        device.next_admit_time_ns = time_ns + config_.max_delay_ns;
        break;
    }
}

PressureLevel classify_pressure(const PressureSample& sample, const BackpressureConfig& config) {
    const auto packet = pressure_ratio(sample.queued_packets, config.queue_capacity_packets);
    const auto payload = pressure_ratio(sample.queued_payload_bytes, config.payload_capacity_bytes);
    const auto ratio = std::max(packet, payload);

    PressureLevel level = PressureLevel::nominal;
    if (ratio >= config.thresholds.stop_ratio) {
        level = PressureLevel::stop;
    } else if (ratio >= config.thresholds.shed_ratio) {
        level = PressureLevel::shed;
    } else if (ratio >= config.thresholds.throttle_ratio) {
        level = PressureLevel::throttle;
    } else if (ratio >= config.thresholds.watch_ratio) {
        level = PressureLevel::watch;
    }

    if (sample.stalled_devices > 0 && level_rank(level) < level_rank(PressureLevel::throttle)) {
        level = PressureLevel::throttle;
    }
    if (sample.inflight_batches > config.queue_capacity_packets / 4 && config.queue_capacity_packets >= 4) {
        level = max_level(level, PressureLevel::watch);
    }
    return level;
}

double pressure_ratio(std::size_t used, std::size_t capacity) noexcept {
    if (capacity == 0) {
        return used == 0 ? 0.0 : 1.0;
    }
    return static_cast<double>(used) / static_cast<double>(capacity);
}

std::string pressure_level_name(PressureLevel level) {
    switch (level) {
    case PressureLevel::nominal:
        return "nominal";
    case PressureLevel::watch:
        return "watch";
    case PressureLevel::throttle:
        return "throttle";
    case PressureLevel::shed:
        return "shed";
    case PressureLevel::stop:
        return "stop";
    }
    return "unknown";
}

std::string admission_action_name(AdmissionAction action) {
    switch (action) {
    case AdmissionAction::accept:
        return "accept";
    case AdmissionAction::delay:
        return "delay";
    case AdmissionAction::downsample:
        return "downsample";
    case AdmissionAction::shed:
        return "shed";
    case AdmissionAction::reject:
        return "reject";
    }
    return "unknown";
}

std::string render_admission_decision(const AdmissionDecision& decision) {
    std::ostringstream out;
    out << "admission_decision\n"
        << "  action: "
        << admission_action_name(decision.action)
        << "\n"
        << "  pressure: "
        << pressure_level_name(decision.pressure)
        << "\n"
        << "  delay_ns: "
        << decision.delay_ns
        << "\n"
        << "  downsample_factor: "
        << decision.downsample_factor
        << "\n"
        << "  reason: "
        << decision.reason
        << "\n";
    return out.str();
}

std::string render_backpressure_snapshot(const BackpressureSnapshot& snapshot) {
    std::ostringstream out;
    out << "backpressure_snapshot\n"
        << "  level: "
        << pressure_level_name(snapshot.level)
        << "\n"
        << "  time_ns: "
        << snapshot.sample.time_ns
        << "\n"
        << "  queued_packets: "
        << snapshot.sample.queued_packets
        << "\n"
        << "  queued_payload_bytes: "
        << snapshot.sample.queued_payload_bytes
        << "\n"
        << "  packet_ratio: "
        << snapshot.packet_ratio
        << "\n"
        << "  payload_ratio: "
        << snapshot.payload_ratio
        << "\n"
        << "  decisions: "
        << snapshot.decisions
        << "\n";
    for (const auto& device : snapshot.devices) {
        out << "  device: "
            << device.device
            << " admitted="
            << device.admitted_packets
            << " delayed="
            << device.delayed_packets
            << " downsampled="
            << device.downsampled_packets
            << " rejected="
            << device.rejected_packets
            << " factor="
            << device.current_downsample_factor
            << "\n";
    }
    return out.str();
}

} // namespace aethon::control
