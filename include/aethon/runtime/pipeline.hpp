#pragma once

#include "aethon/protocol/types.hpp"
#include "aethon/runtime/collector.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aethon::runtime {

enum class PipelineStepResult {
    continue_processing,
    drop_packet,
    stop_pipeline,
};

struct PipelineContext {
    std::uint64_t arrival_time_ns = 0;
    std::vector<std::string> notes;
};

using PipelineStep = std::function<PipelineStepResult(protocol::Packet&, PipelineContext&)>;

struct PipelineRunResult {
    bool accepted = false;
    std::vector<std::string> notes;
};

class PacketPipeline {
public:
    void add_step(std::string name, PipelineStep step);
    [[nodiscard]] PipelineRunResult run(protocol::Packet& packet, std::uint64_t arrival_time_ns) const;
    [[nodiscard]] std::vector<std::string> step_names() const;
    void clear();

private:
    struct StepEntry {
        std::string name;
        PipelineStep step;
    };

    std::vector<StepEntry> steps_;
};

[[nodiscard]] PipelineStep make_payload_limit_step(std::size_t max_payload_size);
[[nodiscard]] PipelineStep make_device_allowlist_step(std::vector<protocol::DeviceId> devices);
[[nodiscard]] std::string pipeline_step_result_name(PipelineStepResult result);
[[nodiscard]] std::string render_pipeline_result(const PipelineRunResult& result);

} // namespace aethon::runtime
