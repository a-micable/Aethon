#include "test_harness.hpp"
#include "aethon/security/signature_note.hpp"

AETHON_TEST(signature_note_evaluates_thresholds) {
    aethon::security::SignatureNote component("signature_note");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_signature_note_decision(decision).empty());
}
