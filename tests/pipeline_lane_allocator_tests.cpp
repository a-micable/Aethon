#include "test_harness.hpp"
#include "aethon/pipeline/lane_allocator.hpp"

AETHON_TEST(lane_allocator_evaluates_thresholds) {
    aethon::pipeline::LaneAllocator component("lane_allocator");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_lane_allocator_decision(decision).empty());
}
