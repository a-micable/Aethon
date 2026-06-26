#include "test_harness.hpp"
#include "aethon/security/quarantine_set.hpp"

AETHON_TEST(quarantine_set_evaluates_thresholds) {
    aethon::security::QuarantineSet component("quarantine_set");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_quarantine_set_decision(decision).empty());
}
