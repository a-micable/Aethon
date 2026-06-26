#include "test_harness.hpp"
#include "aethon/control/device_lock.hpp"

AETHON_TEST(device_lock_evaluates_thresholds) {
    aethon::control::DeviceLock component("device_lock");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_device_lock_decision(decision).empty());
}
