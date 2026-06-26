#include "test_harness.hpp"
#include "aethon/security/trust_score.hpp"

AETHON_TEST(trust_score_evaluates_thresholds) {
    aethon::security::TrustScore component("trust_score");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_trust_score_decision(decision).empty());
}
