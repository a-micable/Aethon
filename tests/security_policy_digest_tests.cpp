#include "test_harness.hpp"
#include "aethon/security/policy_digest.hpp"

AETHON_TEST(policy_digest_evaluates_thresholds) {
    aethon::security::PolicyDigest component("policy_digest");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_policy_digest_decision(decision).empty());
}
