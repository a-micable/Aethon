#include "test_harness.hpp"
#include "aethon/telemetry/message_catalog.hpp"

AETHON_TEST(message_catalog_evaluates_thresholds) {
    aethon::telemetry::MessageCatalog component("message_catalog");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_message_catalog_decision(decision).empty());
}
