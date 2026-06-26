#include "test_harness.hpp"
#include "aethon/control/firmware_rollout.hpp"

AETHON_TEST(firmware_rollout_evaluates_thresholds) {
    aethon::control::FirmwareRollout component("firmware_rollout");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_firmware_rollout_decision(decision).empty());
}
