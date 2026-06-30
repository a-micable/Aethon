#include "aethon/rf/band_plan.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

namespace aethon::rf {
namespace {

constexpr std::uint64_t khz(std::uint64_t value) {
    return value * 1'000ULL;
}

constexpr std::uint64_t mhz(std::uint64_t value) {
    return value * 1'000'000ULL;
}

constexpr std::uint64_t ghz(std::uint64_t value) {
    return value * 1'000'000'000ULL;
}

bool allocation_allowed(const BandAllocation& allocation, const BandPlanQuery& query) {
    if (!query.include_secondary && allocation.status == AllocationStatus::secondary) {
        return false;
    }
    if (!query.include_experimental && allocation.status == AllocationStatus::experimental) {
        return false;
    }
    if (query.service && allocation.service != *query.service) {
        return false;
    }
    if (query.range && !allocation.range.overlaps(*query.range)) {
        return false;
    }
    return true;
}

int allocation_rank(AllocationStatus status) {
    switch (status) {
    case AllocationStatus::primary:
        return 0;
    case AllocationStatus::shared:
        return 1;
    case AllocationStatus::protected_passive:
        return 2;
    case AllocationStatus::secondary:
        return 3;
    case AllocationStatus::experimental:
        return 4;
    }
    return 5;
}

void add_issue(std::vector<BandPlanIssue>& issues,
               std::string code,
               std::string message,
               FrequencyRange range) {
    issues.push_back(BandPlanIssue{std::move(code), std::move(message), range});
}

BandAllocation allocation(std::string name,
                          std::uint64_t lower_hz,
                          std::uint64_t upper_hz,
                          RfService service,
                          AllocationStatus status,
                          DuplexMode duplex,
                          std::uint64_t guard_hz,
                          std::uint64_t channel_width_hz,
                          std::string regulator,
                          std::string notes) {
    BandAllocation result;
    result.name = std::move(name);
    result.range = {lower_hz, upper_hz};
    result.service = service;
    result.status = status;
    result.duplex = duplex;
    result.guard_hz = guard_hz;
    result.expected_channel_width_hz = channel_width_hz;
    result.regulator = std::move(regulator);
    result.notes = std::move(notes);
    return result;
}

ChannelRaster raster(std::uint64_t first_center_hz,
                     std::uint64_t spacing_hz,
                     std::uint64_t bandwidth_hz,
                     std::uint32_t first_channel,
                     std::uint32_t last_channel) {
    return {first_center_hz, spacing_hz, bandwidth_hz, first_channel, last_channel};
}

void add_vhf_uhf_allocations(BandPlan& plan) {
    auto fm = allocation(
        "FM broadcast",
        mhz(87) + khz(500),
        mhz(108),
        RfService::broadcast,
        AllocationStatus::primary,
        DuplexMode::receive_only,
        khz(100),
        khz(200),
        "ITU/R1 monitor",
        "Wideband FM broadcast channels; useful for receiver linearity checks.");
    fm.rasters.push_back(raster(mhz(87) + khz(600), khz(200), khz(180), 1, 102));
    plan.add_allocation(fm);

    auto air = allocation(
        "VHF airband voice",
        mhz(118),
        mhz(137),
        RfService::aeronautical,
        AllocationStatus::primary,
        DuplexMode::simplex,
        khz(25),
        khz(25),
        "ICAO monitor",
        "AM voice, 8.33 kHz or 25 kHz spacing depending on region.");
    air.rasters.push_back(raster(mhz(118), khz(25), khz(8), 0, 759));
    plan.add_allocation(air);

    auto weather = allocation(
        "NOAA weather radio",
        mhz(162) + khz(400),
        mhz(162) + khz(560),
        RfService::weather,
        AllocationStatus::primary,
        DuplexMode::receive_only,
        khz(5),
        khz(25),
        "monitor",
        "Narrow FM weather transmitters, strong references for field checks.");
    weather.rasters.push_back(raster(mhz(162) + khz(400), khz(25), khz(16), 1, 7));
    plan.add_allocation(weather);

    auto marine = allocation(
        "VHF marine",
        mhz(156),
        mhz(163),
        RfService::maritime,
        AllocationStatus::primary,
        DuplexMode::simplex,
        khz(25),
        khz(25),
        "ITU maritime",
        "Marine voice and AIS allocations.");
    marine.rasters.push_back(raster(mhz(156), khz(25), khz(16), 1, 280));
    plan.add_allocation(marine);

    auto vhf_public = allocation(
        "VHF public safety",
        mhz(150),
        mhz(174),
        RfService::public_safety,
        AllocationStatus::shared,
        DuplexMode::simplex,
        khz(12),
        khz(12),
        "regional monitor",
        "Narrowband land mobile and public safety channels.");
    vhf_public.rasters.push_back(raster(mhz(150), khz(12) + 500, khz(11), 0, 1919));
    plan.add_allocation(vhf_public);

    auto uhf_public = allocation(
        "UHF land mobile",
        mhz(420),
        mhz(470),
        RfService::land_mobile,
        AllocationStatus::shared,
        DuplexMode::simplex,
        khz(12),
        khz(12),
        "regional monitor",
        "UHF narrowband business, public safety, and telemetry users.");
    uhf_public.rasters.push_back(raster(mhz(420), khz(12) + 500, khz(11), 0, 4000));
    plan.add_allocation(uhf_public);

    auto amateur_2m = allocation(
        "2 m amateur",
        mhz(144),
        mhz(148),
        RfService::amateur,
        AllocationStatus::primary,
        DuplexMode::simplex,
        khz(20),
        khz(12),
        "IARU monitor",
        "Weak signal, FM repeaters, packet, and satellite sub-bands.");
    amateur_2m.rasters.push_back(raster(mhz(144), khz(12) + 500, khz(11), 0, 319));
    plan.add_allocation(amateur_2m);

    auto amateur_70cm = allocation(
        "70 cm amateur",
        mhz(430),
        mhz(440),
        RfService::amateur,
        AllocationStatus::primary,
        DuplexMode::simplex,
        khz(25),
        khz(12),
        "IARU monitor",
        "Repeaters, satellites, weak-signal, and digital modes.");
    amateur_70cm.rasters.push_back(raster(mhz(430), khz(25), khz(12), 0, 399));
    plan.add_allocation(amateur_70cm);
}

void add_cellular_and_ism_allocations(BandPlan& plan) {
    auto ism_433 = allocation(
        "433 MHz ISM",
        mhz(433) + khz(50),
        mhz(434) + khz(790),
        RfService::ism,
        AllocationStatus::shared,
        DuplexMode::simplex,
        khz(25),
        khz(25),
        "SRD monitor",
        "Short range devices, remote controls, and low power telemetry.");
    ism_433.rasters.push_back(raster(mhz(433) + khz(75), khz(25), khz(20), 0, 68));
    plan.add_allocation(ism_433);

    auto lora_868 = allocation(
        "868 MHz SRD/LoRa",
        mhz(863),
        mhz(870),
        RfService::telemetry,
        AllocationStatus::shared,
        DuplexMode::simplex,
        khz(100),
        khz(125),
        "ETSI monitor",
        "Low power telemetry and LoRaWAN channels.");
    lora_868.rasters.push_back(raster(mhz(863) + khz(100), khz(100), khz(125), 0, 68));
    lora_868.rasters.push_back(raster(mhz(868) + khz(100), khz(200), khz(125), 70, 79));
    plan.add_allocation(lora_868);

    auto gsm_900_down = allocation(
        "900 MHz cellular downlink",
        mhz(925),
        mhz(960),
        RfService::cellular,
        AllocationStatus::primary,
        DuplexMode::fdd_downlink,
        khz(100),
        khz(200),
        "3GPP monitor",
        "Legacy GSM/LTE downlink block.");
    gsm_900_down.rasters.push_back(raster(mhz(925) + khz(100), khz(200), khz(180), 1, 174));
    plan.add_allocation(gsm_900_down);

    auto gsm_900_up = allocation(
        "900 MHz cellular uplink",
        mhz(880),
        mhz(915),
        RfService::cellular,
        AllocationStatus::primary,
        DuplexMode::fdd_uplink,
        khz(100),
        khz(200),
        "3GPP monitor",
        "Legacy GSM/LTE uplink block.");
    gsm_900_up.rasters.push_back(raster(mhz(880) + khz(100), khz(200), khz(180), 1, 174));
    plan.add_allocation(gsm_900_up);

    auto cellular_1800_down = allocation(
        "1800 MHz cellular downlink",
        mhz(1805),
        mhz(1880),
        RfService::cellular,
        AllocationStatus::primary,
        DuplexMode::fdd_downlink,
        khz(100),
        mhz(5),
        "3GPP monitor",
        "LTE/NR carrier blocks.");
    cellular_1800_down.rasters.push_back(raster(mhz(1805), khz(100), mhz(5), 0, 749));
    plan.add_allocation(cellular_1800_down);

    auto ism_24 = allocation(
        "2.4 GHz ISM",
        mhz(2400),
        mhz(2483) + khz(500),
        RfService::ism,
        AllocationStatus::shared,
        DuplexMode::tdd,
        khz(100),
        mhz(20),
        "ISM monitor",
        "Wi-Fi, Bluetooth, microwave ovens, and proprietary telemetry.");
    ism_24.rasters.push_back(raster(mhz(2412), mhz(5), mhz(20), 1, 14));
    plan.add_allocation(ism_24);

    auto ism_58 = allocation(
        "5.8 GHz ISM",
        mhz(5725),
        mhz(5875),
        RfService::ism,
        AllocationStatus::shared,
        DuplexMode::tdd,
        mhz(1),
        mhz(20),
        "ISM monitor",
        "Wi-Fi, telemetry links, and video senders.");
    ism_58.rasters.push_back(raster(mhz(5745), mhz(20), mhz(20), 0, 6));
    plan.add_allocation(ism_58);
}

void add_navigation_satellite_radar_allocations(BandPlan& plan) {
    auto gps_l1 = allocation(
        "GNSS L1",
        mhz(1559),
        mhz(1610),
        RfService::navigation,
        AllocationStatus::protected_passive,
        DuplexMode::receive_only,
        khz(500),
        mhz(2),
        "RNSS monitor",
        "GPS, Galileo, BeiDou, and SBAS L1/E1 signals.");
    gps_l1.rasters.push_back(raster(mhz(1575) + khz(420), mhz(1), mhz(2), 0, 34));
    plan.add_allocation(gps_l1);

    auto adsb = allocation(
        "1090 MHz ADS-B",
        mhz(1089),
        mhz(1091),
        RfService::aeronautical,
        AllocationStatus::primary,
        DuplexMode::receive_only,
        khz(50),
        mhz(1),
        "ICAO monitor",
        "Mode-S and ADS-B replies at 1090 MHz.");
    adsb.rasters.push_back(raster(mhz(1090), mhz(1), mhz(1), 0, 0));
    plan.add_allocation(adsb);

    auto sat_s = allocation(
        "S-band satellite downlink",
        mhz(2200),
        mhz(2290),
        RfService::satellite,
        AllocationStatus::primary,
        DuplexMode::receive_only,
        khz(500),
        mhz(1),
        "space research monitor",
        "Space research, Earth exploration, and telemetry downlinks.");
    sat_s.rasters.push_back(raster(mhz(2200), khz(500), mhz(1), 0, 179));
    plan.add_allocation(sat_s);

    auto radar_l = allocation(
        "L-band radar",
        ghz(1),
        mhz(1400),
        RfService::radar,
        AllocationStatus::primary,
        DuplexMode::transmit_only,
        mhz(1),
        mhz(5),
        "radiolocation monitor",
        "Long-range surveillance and aeronautical radar.");
    radar_l.rasters.push_back(raster(ghz(1), mhz(5), mhz(5), 0, 79));
    plan.add_allocation(radar_l);

    auto weather_passive = allocation(
        "1400 MHz passive weather",
        mhz(1400),
        mhz(1427),
        RfService::weather,
        AllocationStatus::protected_passive,
        DuplexMode::receive_only,
        khz(500),
        mhz(1),
        "EESS passive",
        "Protected passive Earth sensing; emissions here are suspicious.");
    weather_passive.rasters.push_back(raster(mhz(1400), mhz(1), mhz(1), 0, 26));
    plan.add_allocation(weather_passive);
}

} // namespace

BandPlan::BandPlan(std::string name)
    : name_(std::move(name)) {}

void BandPlan::add_allocation(BandAllocation allocation) {
    allocations_.push_back(std::move(allocation));
    allocations_ = sort_allocations(std::move(allocations_));
}

void BandPlan::clear() {
    allocations_.clear();
}

const std::string& BandPlan::name() const noexcept {
    return name_;
}

const std::vector<BandAllocation>& BandPlan::allocations() const noexcept {
    return allocations_;
}

std::vector<BandAllocation> BandPlan::query(const BandPlanQuery& query) const {
    std::vector<BandAllocation> result;
    for (const auto& allocation : allocations_) {
        if (allocation_allowed(allocation, query)) {
            result.push_back(allocation);
        }
    }
    return result;
}

std::vector<BandAllocation> BandPlan::allocations_at(std::uint64_t frequency_hz) const {
    std::vector<BandAllocation> result;
    for (const auto& allocation : allocations_) {
        if (allocation.range.contains(frequency_hz)) {
            result.push_back(allocation);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        auto left_rank = allocation_rank(left.status);
        auto right_rank = allocation_rank(right.status);
        if (left_rank != right_rank) {
            return left_rank < right_rank;
        }
        return left.range.width_hz() < right.range.width_hz();
    });
    return result;
}

std::optional<BandAllocation> BandPlan::best_allocation(std::uint64_t frequency_hz) const {
    auto matches = allocations_at(frequency_hz);
    if (matches.empty()) {
        return std::nullopt;
    }
    return matches.front();
}

std::vector<RfChannel> BandPlan::channels_for(const BandAllocation& allocation) const {
    std::vector<RfChannel> channels;
    for (const auto& raster : allocation.rasters) {
        if (raster.spacing_hz == 0) {
            continue;
        }
        for (auto number = raster.first_channel; number <= raster.last_channel; ++number) {
            auto center = center_frequency_for_channel(raster, number);
            if (!center) {
                continue;
            }
            RfChannel channel;
            channel.number = number;
            channel.center_hz = *center;
            channel.bandwidth_hz = raster.bandwidth_hz;
            channel.duplex = allocation.duplex;
            channel.label = allocation.name + " ch " + std::to_string(number);
            if (is_channel_inside_allocation(channel, allocation)) {
                channels.push_back(std::move(channel));
            }
            if (number == std::numeric_limits<std::uint32_t>::max()) {
                break;
            }
        }
    }
    std::sort(channels.begin(), channels.end(), [](const auto& left, const auto& right) {
        if (left.center_hz != right.center_hz) {
            return left.center_hz < right.center_hz;
        }
        return left.number < right.number;
    });
    return channels;
}

std::optional<RfChannel> BandPlan::nearest_channel(std::uint64_t frequency_hz, RfService service) const {
    std::optional<RfChannel> best;
    std::uint64_t best_delta = std::numeric_limits<std::uint64_t>::max();
    for (const auto& allocation : allocations_) {
        if (service != RfService::unknown && allocation.service != service) {
            continue;
        }
        for (const auto& channel : channels_for(allocation)) {
            auto delta = channel.center_hz > frequency_hz
                ? channel.center_hz - frequency_hz
                : frequency_hz - channel.center_hz;
            if (delta < best_delta) {
                best_delta = delta;
                best = channel;
            }
        }
    }
    return best;
}

std::vector<FrequencyRange> BandPlan::guard_ranges(const BandAllocation& allocation) const {
    std::vector<FrequencyRange> ranges;
    if (allocation.guard_hz == 0 || allocation.range.empty()) {
        return ranges;
    }
    const auto lower_end = std::min(allocation.range.lower_hz + allocation.guard_hz, allocation.range.upper_hz);
    const auto upper_start = allocation.range.upper_hz > allocation.guard_hz
        ? allocation.range.upper_hz - allocation.guard_hz
        : allocation.range.lower_hz;
    if (lower_end > allocation.range.lower_hz) {
        ranges.push_back({allocation.range.lower_hz, lower_end});
    }
    if (allocation.range.upper_hz > upper_start && upper_start > allocation.range.lower_hz) {
        ranges.push_back({upper_start, allocation.range.upper_hz});
    }
    return ranges;
}

std::vector<BandPlanIssue> BandPlan::validate() const {
    std::vector<BandPlanIssue> issues;
    for (const auto& allocation : allocations_) {
        if (allocation.name.empty()) {
            add_issue(issues, "band.name.empty", "allocation has no name", allocation.range);
        }
        if (allocation.range.empty()) {
            add_issue(issues, "band.range.empty", "allocation has an empty frequency range", allocation.range);
        }
        if (allocation.expected_channel_width_hz > allocation.range.width_hz() && !allocation.range.empty()) {
            add_issue(issues, "band.channel.too_wide", "expected channel width exceeds allocation width", allocation.range);
        }
        for (const auto& raster : allocation.rasters) {
            if (raster.spacing_hz == 0) {
                add_issue(issues, "band.raster.spacing_zero", "channel raster spacing is zero", allocation.range);
            }
            if (raster.first_channel > raster.last_channel) {
                add_issue(issues, "band.raster.channel_order", "channel raster first channel is after last channel", allocation.range);
            }
            auto first = center_frequency_for_channel(raster, raster.first_channel);
            auto last = center_frequency_for_channel(raster, raster.last_channel);
            if (first && !allocation.range.contains(*first)) {
                add_issue(issues, "band.raster.first_outside", "first raster channel center is outside allocation", allocation.range);
            }
            if (last && !allocation.range.contains(*last)) {
                add_issue(issues, "band.raster.last_outside", "last raster channel center is outside allocation", allocation.range);
            }
        }
    }

    for (std::size_t i = 1; i < allocations_.size(); ++i) {
        const auto& previous = allocations_[i - 1];
        const auto& current = allocations_[i];
        if (previous.range.overlaps(current.range)
            && previous.status == AllocationStatus::primary
            && current.status == AllocationStatus::primary
            && previous.service != current.service) {
            auto overlap = intersect(previous.range, current.range).value_or(FrequencyRange{});
            add_issue(issues, "band.primary.overlap", "primary allocations overlap across services", overlap);
        }
    }
    return issues;
}

FrequencyRange BandPlan::covered_range() const {
    FrequencyRange result;
    for (const auto& allocation : allocations_) {
        result = merge(result, allocation.range);
    }
    return result;
}

BandPlan make_monitoring_band_plan() {
    BandPlan plan("Aethon monitoring RF band plan");
    add_vhf_uhf_allocations(plan);
    add_cellular_and_ism_allocations(plan);
    add_navigation_satellite_radar_allocations(plan);
    return plan;
}

BandPlan make_empty_band_plan(std::string name) {
    return BandPlan(std::move(name));
}

std::vector<BandAllocation> sort_allocations(std::vector<BandAllocation> allocations) {
    std::sort(allocations.begin(), allocations.end(), [](const auto& left, const auto& right) {
        if (left.range.lower_hz != right.range.lower_hz) {
            return left.range.lower_hz < right.range.lower_hz;
        }
        if (left.range.upper_hz != right.range.upper_hz) {
            return left.range.upper_hz < right.range.upper_hz;
        }
        return left.name < right.name;
    });
    return allocations;
}

bool is_channel_inside_allocation(const RfChannel& channel, const BandAllocation& allocation) {
    auto half = channel.bandwidth_hz / 2;
    FrequencyRange occupied{
        channel.center_hz > half ? channel.center_hz - half : 0,
        channel.center_hz + half + (channel.bandwidth_hz % 2),
    };
    return allocation.range.contains(channel.center_hz) && occupied.lower_hz >= allocation.range.lower_hz && occupied.upper_hz <= allocation.range.upper_hz;
}

std::optional<std::uint32_t> channel_number_for_frequency(const ChannelRaster& raster,
                                                          std::uint64_t frequency_hz) {
    if (raster.spacing_hz == 0 || frequency_hz < raster.first_center_hz) {
        return std::nullopt;
    }
    auto offset = frequency_hz - raster.first_center_hz;
    auto nearest = (offset + raster.spacing_hz / 2) / raster.spacing_hz;
    if (nearest > std::numeric_limits<std::uint32_t>::max() - raster.first_channel) {
        return std::nullopt;
    }
    auto channel = raster.first_channel + static_cast<std::uint32_t>(nearest);
    if (channel < raster.first_channel || channel > raster.last_channel) {
        return std::nullopt;
    }
    return channel;
}

std::optional<std::uint64_t> center_frequency_for_channel(const ChannelRaster& raster,
                                                          std::uint32_t channel) {
    if (raster.spacing_hz == 0 || channel < raster.first_channel || channel > raster.last_channel) {
        return std::nullopt;
    }
    auto offset = static_cast<std::uint64_t>(channel - raster.first_channel);
    if (offset > (std::numeric_limits<std::uint64_t>::max() - raster.first_center_hz) / raster.spacing_hz) {
        return std::nullopt;
    }
    return raster.first_center_hz + offset * raster.spacing_hz;
}

std::string duplex_mode_name(DuplexMode mode) {
    switch (mode) {
    case DuplexMode::simplex:
        return "simplex";
    case DuplexMode::fdd_downlink:
        return "fdd_downlink";
    case DuplexMode::fdd_uplink:
        return "fdd_uplink";
    case DuplexMode::tdd:
        return "tdd";
    case DuplexMode::receive_only:
        return "receive_only";
    case DuplexMode::transmit_only:
        return "transmit_only";
    }
    return "simplex";
}

std::string allocation_status_name(AllocationStatus status) {
    switch (status) {
    case AllocationStatus::primary:
        return "primary";
    case AllocationStatus::secondary:
        return "secondary";
    case AllocationStatus::shared:
        return "shared";
    case AllocationStatus::experimental:
        return "experimental";
    case AllocationStatus::protected_passive:
        return "protected_passive";
    }
    return "primary";
}

std::string render_allocation(const BandAllocation& allocation) {
    std::ostringstream out;
    out << allocation.name
        << " "
        << format_range(allocation.range)
        << " service="
        << rf_service_name(allocation.service)
        << " status="
        << allocation_status_name(allocation.status)
        << " duplex="
        << duplex_mode_name(allocation.duplex)
        << " guard="
        << format_frequency(allocation.guard_hz)
        << " channel_width="
        << format_frequency(allocation.expected_channel_width_hz);
    if (!allocation.regulator.empty()) {
        out << " regulator=" << allocation.regulator;
    }
    if (!allocation.notes.empty()) {
        out << " notes=\"" << allocation.notes << "\"";
    }
    return out.str();
}

std::string render_band_plan(const BandPlan& plan) {
    std::ostringstream out;
    out << "band_plan "
        << plan.name()
        << "\n"
        << "  allocations: "
        << plan.allocations().size()
        << "\n"
        << "  covered: "
        << format_range(plan.covered_range())
        << "\n";
    for (const auto& allocation : plan.allocations()) {
        out << "  allocation: " << render_allocation(allocation) << "\n";
        for (const auto& raster : allocation.rasters) {
            out << "    raster first="
                << format_frequency(raster.first_center_hz)
                << " spacing="
                << format_frequency(raster.spacing_hz)
                << " bandwidth="
                << format_frequency(raster.bandwidth_hz)
                << " channels="
                << raster.first_channel
                << "-"
                << raster.last_channel
                << "\n";
        }
    }
    return out.str();
}

std::string render_band_plan_issues(const std::vector<BandPlanIssue>& issues) {
    std::ostringstream out;
    out << "band_plan_issues\n"
        << "  count: "
        << issues.size()
        << "\n";
    for (const auto& issue : issues) {
        out << "  issue: "
            << issue.code
            << " "
            << format_range(issue.range)
            << " "
            << issue.message
            << "\n";
    }
    return out.str();
}

} // namespace aethon::rf
