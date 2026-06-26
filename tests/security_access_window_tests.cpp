#include "test_harness.hpp"
#include "aethon/security/access_window.hpp"

AETHON_TEST(access_window_evaluates_thresholds) {
    aethon::security::AccessWindow component("access_window");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_access_window_decision(decision).empty());
}
