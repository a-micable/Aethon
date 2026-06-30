#include "aethon/runtime/pipeline.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::runtime {

void PacketPipeline::add_step(std::string name, PipelineStep step) {
    steps_.push_back(StepEntry{
        std::move(name),
        std::move(step),
    });
}

PipelineRunResult PacketPipeline::run(protocol::Packet& packet,
                                      std::uint64_t arrival_time_ns) const {
    PipelineContext context;
    context.arrival_time_ns = arrival_time_ns;
    for (const auto& step : steps_) {
        auto result = step.step(packet, context);
        context.notes.push_back(step.name + ": " + pipeline_step_result_name(result));
        if (result == PipelineStepResult::drop_packet) {
            return PipelineRunResult{
                false,
                context.notes,
            };
        }
        if (result == PipelineStepResult::stop_pipeline) {
            break;
        }
    }
    return PipelineRunResult{
        true,
        context.notes,
    };
}

std::vector<std::string> PacketPipeline::step_names() const {
    std::vector<std::string> names;
    names.reserve(steps_.size());
    for (const auto& step : steps_) {
        names.push_back(step.name);
    }
    return names;
}

void PacketPipeline::clear() {
    steps_.clear();
}

PipelineStep make_payload_limit_step(std::size_t max_payload_size) {
    return [max_payload_size](protocol::Packet& packet, PipelineContext& context) {
        if (packet.payload.size() > max_payload_size) {
            context.notes.push_back("payload limit exceeded");
            return PipelineStepResult::drop_packet;
        }
        return PipelineStepResult::continue_processing;
    };
}

PipelineStep make_device_allowlist_step(std::vector<protocol::DeviceId> devices) {
    std::sort(devices.begin(), devices.end());
    return [devices = std::move(devices)](protocol::Packet& packet, PipelineContext& context) {
        if (!std::binary_search(devices.begin(), devices.end(), packet.device)) {
            context.notes.push_back("device is not in allowlist");
            return PipelineStepResult::drop_packet;
        }
        return PipelineStepResult::continue_processing;
    };
}

std::string pipeline_step_result_name(PipelineStepResult result) {
    switch (result) {
    case PipelineStepResult::continue_processing:
        return "continue";
    case PipelineStepResult::drop_packet:
        return "drop";
    case PipelineStepResult::stop_pipeline:
        return "stop";
    }
    return "unknown";
}

std::string render_pipeline_result(const PipelineRunResult& result) {
    std::ostringstream out;
    out << "pipeline_result\n"
        << "  accepted: "
        << (result.accepted ? "yes" : "no")
        << "\n";
    for (const auto& note : result.notes) {
        out << "  note: "
            << note
            << "\n";
    }
    return out.str();
}

} // namespace aethon::runtime
