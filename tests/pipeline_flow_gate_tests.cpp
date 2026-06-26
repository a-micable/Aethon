#include "test_harness.hpp"
#include "aethon/pipeline/flow_gate.hpp"

AETHON_TEST(flow_gate_evaluates_thresholds) {
    aethon::pipeline::FlowGate component("flow_gate");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_flow_gate_decision(decision).empty());
}
