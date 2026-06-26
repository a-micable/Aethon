#include "test_harness.hpp"
#include "aethon/query/time_partition.hpp"

AETHON_TEST(time_partition_evaluates_thresholds) {
    aethon::query::TimePartition component("time_partition");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_time_partition_decision(decision).empty());
}
