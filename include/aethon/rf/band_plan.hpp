#pragma once

#include "aethon/rf/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aethon::rf {

enum class DuplexMode : std::uint8_t {
    simplex = 0,
    fdd_downlink = 1,
    fdd_uplink = 2,
    tdd = 3,
    receive_only = 4,
    transmit_only = 5,
};

enum class AllocationStatus : std::uint8_t {
    primary = 0,
    secondary = 1,
    shared = 2,
    experimental = 3,
    protected_passive = 4,
};

struct ChannelRaster {
    std::uint64_t first_center_hz = 0;
    std::uint64_t spacing_hz = 0;
    std::uint64_t bandwidth_hz = 0;
    std::uint32_t first_channel = 0;
    std::uint32_t last_channel = 0;
};

struct RfChannel {
    std::uint32_t number = 0;
    std::uint64_t center_hz = 0;
    std::uint64_t bandwidth_hz = 0;
    DuplexMode duplex = DuplexMode::simplex;
    std::string label;
};

struct BandAllocation {
    std::string name;
    FrequencyRange range;
    RfService service = RfService::unknown;
    AllocationStatus status = AllocationStatus::primary;
    DuplexMode duplex = DuplexMode::simplex;
    std::uint64_t guard_hz = 0;
    std::uint64_t expected_channel_width_hz = 0;
    std::string regulator;
    std::string notes;
    std::vector<ChannelRaster> rasters;
};

struct BandPlanIssue {
    std::string code;
    std::string message;
    FrequencyRange range;
};

struct BandPlanQuery {
    std::optional<FrequencyRange> range;
    std::optional<RfService> service;
    bool include_secondary = true;
    bool include_experimental = true;
};

class BandPlan {
public:
    explicit BandPlan(std::string name = {});

    void add_allocation(BandAllocation allocation);
    void clear();

    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<BandAllocation>& allocations() const noexcept;
    [[nodiscard]] std::vector<BandAllocation> query(const BandPlanQuery& query) const;
    [[nodiscard]] std::vector<BandAllocation> allocations_at(std::uint64_t frequency_hz) const;
    [[nodiscard]] std::optional<BandAllocation> best_allocation(std::uint64_t frequency_hz) const;
    [[nodiscard]] std::vector<RfChannel> channels_for(const BandAllocation& allocation) const;
    [[nodiscard]] std::optional<RfChannel> nearest_channel(std::uint64_t frequency_hz,
                                                          RfService service = RfService::unknown) const;
    [[nodiscard]] std::vector<FrequencyRange> guard_ranges(const BandAllocation& allocation) const;
    [[nodiscard]] std::vector<BandPlanIssue> validate() const;
    [[nodiscard]] FrequencyRange covered_range() const;

private:
    std::string name_;
    std::vector<BandAllocation> allocations_;
};

[[nodiscard]] BandPlan make_monitoring_band_plan();
[[nodiscard]] BandPlan make_empty_band_plan(std::string name);
[[nodiscard]] std::vector<BandAllocation> sort_allocations(std::vector<BandAllocation> allocations);
[[nodiscard]] bool is_channel_inside_allocation(const RfChannel& channel, const BandAllocation& allocation);
[[nodiscard]] std::optional<std::uint32_t> channel_number_for_frequency(const ChannelRaster& raster,
                                                                        std::uint64_t frequency_hz);
[[nodiscard]] std::optional<std::uint64_t> center_frequency_for_channel(const ChannelRaster& raster,
                                                                        std::uint32_t channel);
[[nodiscard]] std::string duplex_mode_name(DuplexMode mode);
[[nodiscard]] std::string allocation_status_name(AllocationStatus status);
[[nodiscard]] std::string render_allocation(const BandAllocation& allocation);
[[nodiscard]] std::string render_band_plan(const BandPlan& plan);
[[nodiscard]] std::string render_band_plan_issues(const std::vector<BandPlanIssue>& issues);

} // namespace aethon::rf
