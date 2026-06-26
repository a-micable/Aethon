#include "test_harness.hpp"
#include "aethon/query/field_projection.hpp"

AETHON_TEST(field_projection_evaluates_thresholds) {
    aethon::query::FieldProjection component("field_projection");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_field_projection_decision(decision).empty());
}
