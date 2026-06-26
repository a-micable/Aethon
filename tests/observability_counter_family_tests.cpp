#include "test_harness.hpp"
#include "aethon/observability/counter_family.hpp"

AETHON_TEST(counter_family_evaluates_thresholds) {
    aethon::observability::CounterFamily component("counter_family");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_counter_family_decision(decision).empty());
}
