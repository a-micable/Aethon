#include "test_harness.hpp"
#include "aethon/routing/dead_letter.hpp"

AETHON_TEST(dead_letter_summarizes_weighted_samples) {
    aethon::routing::DeadLetterQueue component("dead_letter");
    component.observe({10, 2.0, 1.0, "alpha"});
    component.observe({20, 8.0, 3.0, "beta"});
    auto summary = component.summarize();
    AETHON_REQUIRE(summary.count == 2);
    AETHON_REQUIRE(summary.maximum >= 8.0);
    AETHON_REQUIRE(!component.select(5.0).empty());
}
