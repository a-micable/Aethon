#pragma once
#include "aethon/storage/archive.hpp"
#include <functional>
namespace aethon::replay {
struct ReplayOptions { double speed = 1.0; std::uint64_t start_time_ns = 0; std::uint64_t end_time_ns = 0; bool preserve_timing = false; };
struct ReplayStats { std::uint64_t records_seen = 0; std::uint64_t records_emitted = 0; std::uint64_t bytes_emitted = 0; };
class ReplayEngine { public: using Handler = std::function<void(const storage::ArchiveRecord&)>; explicit ReplayEngine(ReplayOptions options = {}); ReplayStats run(storage::ArchiveReader& reader, Handler handler); private: bool within_window(std::uint64_t ts) const; ReplayOptions options_; };
} // namespace aethon::replay
