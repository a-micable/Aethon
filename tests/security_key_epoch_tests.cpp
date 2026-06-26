#include "test_harness.hpp"
#include "aethon/security/key_epoch.hpp"

AETHON_TEST(key_epoch_evaluates_thresholds) {
    aethon::security::KeyEpoch component("key_epoch");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_key_epoch_decision(decision).empty());
}
