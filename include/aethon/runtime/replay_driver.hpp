#pragma once

#include "aethon/replay/replay_engine.hpp"
#include "aethon/runtime/event_dispatcher.hpp"

#include <filesystem>
#include <string>

namespace aethon::runtime {

struct ReplayDriverResult {
    replay::ReplayStats stats;
    std::uint64_t events_published = 0;
};

class ReplayDriver {
public:
    explicit ReplayDriver(replay::ReplayOptions options = {});

    void subscribe(RuntimeEventHandler handler);
    [[nodiscard]] ReplayDriverResult run(const std::filesystem::path& archive_path);

private:
    replay::ReplayOptions options_;
    EventDispatcher dispatcher_;
};

[[nodiscard]] std::string render_replay_driver_result(const ReplayDriverResult& result);

} // namespace aethon::runtime
