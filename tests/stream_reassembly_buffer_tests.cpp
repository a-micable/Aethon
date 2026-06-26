#include "test_harness.hpp"
#include "aethon/stream/reassembly_buffer.hpp"

AETHON_TEST(reassembly_buffer_summarizes_weighted_samples) {
    aethon::stream::ReassemblyBuffer component("reassembly_buffer");
    component.observe({10, 2.0, 1.0, "alpha"});
    component.observe({20, 8.0, 3.0, "beta"});
    auto summary = component.summarize();
    AETHON_REQUIRE(summary.count == 2);
    AETHON_REQUIRE(summary.maximum >= 8.0);
    AETHON_REQUIRE(!component.select(5.0).empty());
}
