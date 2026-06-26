#include "test_harness.hpp"
#include "aethon/rf/phase_model.hpp"

AETHON_TEST(phase_model_evaluates_thresholds) {
    aethon::rf::PhaseModel component("phase_model");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_phase_model_decision(decision).empty());
}
