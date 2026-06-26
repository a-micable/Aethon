#include "test_harness.hpp"
#include "aethon/observability/span_sampler.hpp"

AETHON_TEST(span_sampler_evaluates_thresholds) {
    aethon::observability::SpanSampler component("span_sampler");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_span_sampler_decision(decision).empty());
}
