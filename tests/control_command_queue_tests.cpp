#include "test_harness.hpp"
#include "aethon/control/command_queue.hpp"

AETHON_TEST(command_queue_evaluates_thresholds) {
    aethon::control::CommandQueue component("command_queue");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_command_queue_decision(decision).empty());
}
