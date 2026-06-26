#include "aethon/pipeline/flow_gate.hpp"
#include "aethon/query/predicate_plan.hpp"
#include "aethon/routing/route_table.hpp"
#include "aethon/telemetry/event_window.hpp"

#include <cstddef>
#include <cstdint>

namespace {

double scaled(std::uint8_t value) {
    return static_cast<double>(value) / 255.0;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    aethon::pipeline::FlowGate flow("state-flow");
    aethon::query::PredicatePlan predicate("state-query");
    aethon::routing::RouteTable routes("state-route");
    aethon::telemetry::EventWindow events("state-events");

    for (std::size_t i = 0; i + 3 < size && i < 4096; i += 4) {
        std::string key = (data[i] & 1) ? "primary" : "backup";
        auto tick = static_cast<std::uint64_t>(data[i + 1]) + i;
        auto score = scaled(data[i + 2]);
        flow.insert({tick, static_cast<std::uint32_t>(i), score, key, "flow"});
        predicate.insert({tick, static_cast<std::uint32_t>(i + 1), scaled(data[i + 3]), key, "predicate"});
        routes.observe({tick, score * 100.0, 1.0, key});
        events.insert({tick, static_cast<std::uint32_t>(i + 2), score, key, "event"});
        if ((data[i] & 0x20) != 0) {
            flow.remove_before(tick / 2);
            predicate.remove_before(tick / 3);
        }
    }

    (void)flow.evaluate("primary", 0.5);
    (void)predicate.evaluate("backup", 0.25);
    (void)routes.summarize();
    (void)events.drain(0.75);
    return 0;
}
