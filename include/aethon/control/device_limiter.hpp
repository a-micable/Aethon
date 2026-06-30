#pragma once

#include "aethon/protocol/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace aethon::control {

using DeviceLeaseId = std::uint64_t;

enum class DeviceLeaseMode : std::uint8_t {
    shared,
    exclusive,
};

enum class DeviceLimitDecision : std::uint8_t {
    allowed,
    delayed,
    locked,
    rate_limited,
    unknown_device,
};

struct DeviceRateLimit {
    std::uint32_t burst = 16;
    std::uint32_t refill_per_second = 16;
    std::uint32_t minimum_tokens = 1;
};

struct DeviceLimiterConfig {
    DeviceRateLimit default_limit;
    std::uint64_t default_lease_ttl_ns = 5'000'000'000;
    bool allow_unknown_devices = true;
};

struct DeviceLeaseRequest {
    protocol::DeviceId device = 0;
    DeviceLeaseMode mode = DeviceLeaseMode::exclusive;
    std::uint64_t now_ns = 0;
    std::uint64_t ttl_ns = 0;
    std::string owner;
};

struct DeviceLease {
    DeviceLeaseId id = 0;
    protocol::DeviceId device = 0;
    DeviceLeaseMode mode = DeviceLeaseMode::exclusive;
    std::uint64_t acquired_time_ns = 0;
    std::uint64_t expires_time_ns = 0;
    std::string owner;
};

struct DeviceLimitResult {
    DeviceLimitDecision decision = DeviceLimitDecision::allowed;
    std::uint64_t retry_after_ns = 0;
    std::string reason;
};

struct DeviceLimiterStats {
    std::uint64_t allowed = 0;
    std::uint64_t delayed = 0;
    std::uint64_t locked = 0;
    std::uint64_t rate_limited = 0;
    std::uint64_t leases_granted = 0;
    std::uint64_t leases_released = 0;
    std::uint64_t leases_expired = 0;
};

struct DeviceLimiterSnapshot {
    protocol::DeviceId device = 0;
    std::uint32_t tokens = 0;
    std::uint64_t last_refill_ns = 0;
    std::vector<DeviceLease> leases;
};

class DeviceLimiter {
public:
    explicit DeviceLimiter(DeviceLimiterConfig config = {});

    [[nodiscard]] const DeviceLimiterConfig& config() const noexcept;
    [[nodiscard]] DeviceLimiterStats stats() const noexcept;
    [[nodiscard]] std::vector<DeviceLimiterSnapshot> snapshot() const;

    void register_device(protocol::DeviceId device, DeviceRateLimit limit = {});
    void unregister_device(protocol::DeviceId device);
    void set_limit(protocol::DeviceId device, DeviceRateLimit limit);

    [[nodiscard]] std::optional<DeviceLease> acquire(DeviceLeaseRequest request);
    [[nodiscard]] bool release(DeviceLeaseId lease_id);
    [[nodiscard]] std::size_t expire(std::uint64_t now_ns);
    [[nodiscard]] DeviceLimitResult check(protocol::DeviceId device,
                                          std::uint32_t cost,
                                          std::uint64_t now_ns);
    [[nodiscard]] DeviceLimitResult consume(protocol::DeviceId device,
                                            std::uint32_t cost,
                                            std::uint64_t now_ns);
    void reset();

private:
    struct DeviceBucket {
        DeviceRateLimit limit;
        std::uint32_t tokens = 0;
        std::uint64_t last_refill_ns = 0;
        std::vector<DeviceLease> leases;
    };

    [[nodiscard]] DeviceBucket* find_bucket(protocol::DeviceId device);
    [[nodiscard]] const DeviceBucket* find_bucket(protocol::DeviceId device) const;
    [[nodiscard]] bool lease_conflict(const DeviceBucket& bucket, DeviceLeaseMode mode) const;
    [[nodiscard]] std::uint64_t retry_after(const DeviceBucket& bucket, std::uint32_t cost) const;
    void refill(DeviceBucket& bucket, std::uint64_t now_ns);
    void erase_lease(DeviceBucket& bucket, DeviceLeaseId lease_id);
    [[nodiscard]] DeviceLeaseId next_lease_id();

    DeviceLimiterConfig config_;
    std::map<protocol::DeviceId, DeviceBucket> devices_;
    DeviceLimiterStats stats_;
    DeviceLeaseId next_lease_id_ = 1;
};

[[nodiscard]] std::string device_lease_mode_name(DeviceLeaseMode mode);
[[nodiscard]] std::string device_limit_decision_name(DeviceLimitDecision decision);
[[nodiscard]] std::string render_device_limit_result(const DeviceLimitResult& result);
[[nodiscard]] std::string render_device_limiter_snapshot(const std::vector<DeviceLimiterSnapshot>& snapshots);

} // namespace aethon::control
