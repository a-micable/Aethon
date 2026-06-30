#include "aethon/control/device_limiter.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::control {
namespace {

constexpr std::uint64_t one_second_ns = 1'000'000'000;

DeviceRateLimit normalized(DeviceRateLimit limit) {
    if (limit.burst == 0) {
        limit.burst = 1;
    }
    if (limit.refill_per_second == 0) {
        limit.refill_per_second = 1;
    }
    if (limit.minimum_tokens == 0) {
        limit.minimum_tokens = 1;
    }
    if (limit.minimum_tokens > limit.burst) {
        limit.minimum_tokens = limit.burst;
    }
    return limit;
}

} // namespace

DeviceLimiter::DeviceLimiter(DeviceLimiterConfig config)
    : config_(config) {
    config_.default_limit = normalized(config_.default_limit);
}

const DeviceLimiterConfig& DeviceLimiter::config() const noexcept {
    return config_;
}

DeviceLimiterStats DeviceLimiter::stats() const noexcept {
    return stats_;
}

std::vector<DeviceLimiterSnapshot> DeviceLimiter::snapshot() const {
    std::vector<DeviceLimiterSnapshot> snapshots;
    snapshots.reserve(devices_.size());
    for (const auto& [device, bucket] : devices_) {
        snapshots.push_back(DeviceLimiterSnapshot{
            device,
            bucket.tokens,
            bucket.last_refill_ns,
            bucket.leases,
        });
    }
    return snapshots;
}

void DeviceLimiter::register_device(protocol::DeviceId device, DeviceRateLimit limit) {
    if (limit.burst == 0 && limit.refill_per_second == 0) {
        limit = config_.default_limit;
    }
    auto& bucket = devices_[device];
    bucket.limit = normalized(limit);
    bucket.tokens = bucket.limit.burst;
}

void DeviceLimiter::unregister_device(protocol::DeviceId device) {
    devices_.erase(device);
}

void DeviceLimiter::set_limit(protocol::DeviceId device, DeviceRateLimit limit) {
    auto& bucket = devices_[device];
    bucket.limit = normalized(limit);
    bucket.tokens = std::min(bucket.tokens, bucket.limit.burst);
}

std::optional<DeviceLease> DeviceLimiter::acquire(DeviceLeaseRequest request) {
    auto* bucket = find_bucket(request.device);
    if (!bucket) {
        if (!config_.allow_unknown_devices) {
            return std::nullopt;
        }
        register_device(request.device, config_.default_limit);
        bucket = find_bucket(request.device);
    }
    if (!bucket) {
        return std::nullopt;
    }
    (void)expire(request.now_ns);
    if (lease_conflict(*bucket, request.mode)) {
        ++stats_.locked;
        return std::nullopt;
    }

    DeviceLease lease;
    lease.id = next_lease_id();
    lease.device = request.device;
    lease.mode = request.mode;
    lease.acquired_time_ns = request.now_ns;
    lease.expires_time_ns = request.now_ns + (request.ttl_ns == 0 ? config_.default_lease_ttl_ns : request.ttl_ns);
    lease.owner = std::move(request.owner);
    bucket->leases.push_back(lease);
    ++stats_.leases_granted;
    return lease;
}

bool DeviceLimiter::release(DeviceLeaseId lease_id) {
    for (auto& [unused, bucket] : devices_) {
        (void)unused;
        const auto before = bucket.leases.size();
        erase_lease(bucket, lease_id);
        if (bucket.leases.size() != before) {
            ++stats_.leases_released;
            return true;
        }
    }
    return false;
}

std::size_t DeviceLimiter::expire(std::uint64_t now_ns) {
    std::size_t expired = 0;
    for (auto& [unused, bucket] : devices_) {
        (void)unused;
        const auto before = bucket.leases.size();
        bucket.leases.erase(
            std::remove_if(bucket.leases.begin(), bucket.leases.end(), [now_ns](const DeviceLease& lease) {
                return lease.expires_time_ns <= now_ns;
            }),
            bucket.leases.end());
        expired += before - bucket.leases.size();
    }
    stats_.leases_expired += expired;
    return expired;
}

DeviceLimitResult DeviceLimiter::check(protocol::DeviceId device,
                                       std::uint32_t cost,
                                       std::uint64_t now_ns) {
    auto* bucket = find_bucket(device);
    if (!bucket) {
        if (!config_.allow_unknown_devices) {
            ++stats_.locked;
            return DeviceLimitResult{DeviceLimitDecision::unknown_device, 0, "device is not registered"};
        }
        register_device(device, config_.default_limit);
        bucket = find_bucket(device);
    }
    if (!bucket) {
        return DeviceLimitResult{DeviceLimitDecision::unknown_device, 0, "device bucket unavailable"};
    }

    (void)expire(now_ns);
    refill(*bucket, now_ns);
    if (lease_conflict(*bucket, DeviceLeaseMode::shared)) {
        ++stats_.locked;
        return DeviceLimitResult{DeviceLimitDecision::locked, config_.default_lease_ttl_ns, "device is exclusively locked"};
    }

    const auto effective_cost = std::max(cost, bucket->limit.minimum_tokens);
    if (bucket->tokens < effective_cost) {
        ++stats_.rate_limited;
        return DeviceLimitResult{
            DeviceLimitDecision::rate_limited,
            retry_after(*bucket, effective_cost),
            "device token bucket is empty",
        };
    }

    ++stats_.allowed;
    return DeviceLimitResult{DeviceLimitDecision::allowed, 0, "device allowed"};
}

DeviceLimitResult DeviceLimiter::consume(protocol::DeviceId device,
                                         std::uint32_t cost,
                                         std::uint64_t now_ns) {
    auto result = check(device, cost, now_ns);
    if (result.decision != DeviceLimitDecision::allowed) {
        return result;
    }
    auto* bucket = find_bucket(device);
    if (!bucket) {
        return DeviceLimitResult{DeviceLimitDecision::unknown_device, 0, "device bucket unavailable"};
    }
    const auto effective_cost = std::max(cost, bucket->limit.minimum_tokens);
    if (bucket->tokens >= effective_cost) {
        bucket->tokens -= effective_cost;
    } else {
        bucket->tokens = 0;
    }
    return result;
}

void DeviceLimiter::reset() {
    devices_.clear();
    stats_ = {};
    next_lease_id_ = 1;
}

DeviceLimiter::DeviceBucket* DeviceLimiter::find_bucket(protocol::DeviceId device) {
    auto found = devices_.find(device);
    if (found == devices_.end()) {
        return nullptr;
    }
    return &found->second;
}

const DeviceLimiter::DeviceBucket* DeviceLimiter::find_bucket(protocol::DeviceId device) const {
    auto found = devices_.find(device);
    if (found == devices_.end()) {
        return nullptr;
    }
    return &found->second;
}

bool DeviceLimiter::lease_conflict(const DeviceBucket& bucket, DeviceLeaseMode mode) const {
    if (bucket.leases.empty()) {
        return false;
    }
    if (mode == DeviceLeaseMode::exclusive) {
        return true;
    }
    return std::any_of(bucket.leases.begin(), bucket.leases.end(), [](const DeviceLease& lease) {
        return lease.mode == DeviceLeaseMode::exclusive;
    });
}

std::uint64_t DeviceLimiter::retry_after(const DeviceBucket& bucket, std::uint32_t cost) const {
    if (bucket.limit.refill_per_second == 0) {
        return one_second_ns;
    }
    const auto missing = cost > bucket.tokens ? cost - bucket.tokens : 0;
    if (missing == 0) {
        return 0;
    }
    const auto numerator = static_cast<std::uint64_t>(missing) * one_second_ns;
    return (numerator + bucket.limit.refill_per_second - 1) / bucket.limit.refill_per_second;
}

void DeviceLimiter::refill(DeviceBucket& bucket, std::uint64_t now_ns) {
    if (bucket.last_refill_ns == 0) {
        bucket.last_refill_ns = now_ns;
        if (bucket.tokens == 0) {
            bucket.tokens = bucket.limit.burst;
        }
        return;
    }
    if (now_ns <= bucket.last_refill_ns) {
        return;
    }
    const auto elapsed = now_ns - bucket.last_refill_ns;
    const auto gained = (elapsed * bucket.limit.refill_per_second) / one_second_ns;
    if (gained == 0) {
        return;
    }
    bucket.tokens = std::min<std::uint32_t>(
        bucket.limit.burst,
        bucket.tokens + static_cast<std::uint32_t>(std::min<std::uint64_t>(gained, bucket.limit.burst)));
    bucket.last_refill_ns = now_ns;
}

void DeviceLimiter::erase_lease(DeviceBucket& bucket, DeviceLeaseId lease_id) {
    bucket.leases.erase(
        std::remove_if(bucket.leases.begin(), bucket.leases.end(), [lease_id](const DeviceLease& lease) {
            return lease.id == lease_id;
        }),
        bucket.leases.end());
}

DeviceLeaseId DeviceLimiter::next_lease_id() {
    return next_lease_id_++;
}

std::string device_lease_mode_name(DeviceLeaseMode mode) {
    switch (mode) {
    case DeviceLeaseMode::shared:
        return "shared";
    case DeviceLeaseMode::exclusive:
        return "exclusive";
    }
    return "unknown";
}

std::string device_limit_decision_name(DeviceLimitDecision decision) {
    switch (decision) {
    case DeviceLimitDecision::allowed:
        return "allowed";
    case DeviceLimitDecision::delayed:
        return "delayed";
    case DeviceLimitDecision::locked:
        return "locked";
    case DeviceLimitDecision::rate_limited:
        return "rate_limited";
    case DeviceLimitDecision::unknown_device:
        return "unknown_device";
    }
    return "unknown";
}

std::string render_device_limit_result(const DeviceLimitResult& result) {
    std::ostringstream out;
    out << "device_limit_result\n"
        << "  decision: "
        << device_limit_decision_name(result.decision)
        << "\n"
        << "  retry_after_ns: "
        << result.retry_after_ns
        << "\n"
        << "  reason: "
        << result.reason
        << "\n";
    return out.str();
}

std::string render_device_limiter_snapshot(const std::vector<DeviceLimiterSnapshot>& snapshots) {
    std::ostringstream out;
    out << "device_limiter\n";
    for (const auto& snapshot : snapshots) {
        out << "  device: "
            << snapshot.device
            << " tokens="
            << snapshot.tokens
            << " last_refill_ns="
            << snapshot.last_refill_ns
            << "\n";
        for (const auto& lease : snapshot.leases) {
            out << "    lease: id="
                << lease.id
                << " mode="
                << device_lease_mode_name(lease.mode)
                << " owner=\""
                << lease.owner
                << "\" expires_time_ns="
                << lease.expires_time_ns
                << "\n";
        }
    }
    return out.str();
}

} // namespace aethon::control
