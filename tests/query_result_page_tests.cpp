#include "test_harness.hpp"
#include "aethon/query/result_page.hpp"

AETHON_TEST(result_page_evaluates_thresholds) {
    aethon::query::ResultPage component("result_page");
    component.insert({10, 1, 0.25, "primary", "low"});
    component.insert({20, 2, 0.95, "primary", "high"});
    auto decision = component.evaluate("primary", 0.50);
    AETHON_REQUIRE(decision.accepted);
    AETHON_REQUIRE(component.find("primary").has_value());
    AETHON_REQUIRE(!render_result_page_decision(decision).empty());
}
