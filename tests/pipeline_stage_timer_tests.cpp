#include "test_harness.hpp"
#include "aethon/pipeline/stage_timer.hpp"

AETHON_TEST(stage_timer_evaluates_thresholds) {
    aethon::pipeline::StageTimer component("stage_timer");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_stage_timer_decision(decision).empty());
}
