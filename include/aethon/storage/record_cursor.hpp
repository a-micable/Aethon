#pragma once

#include "aethon/storage/archive.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace aethon::storage {

struct CursorPosition {
    std::uint64_t records_seen = 0;
    std::uint64_t last_capture_time_ns = 0;
    std::uint64_t last_sequence = 0;
};

class RecordCursor {
public:
    explicit RecordCursor(std::filesystem::path path);

    [[nodiscard]] std::optional<ArchiveRecord> next();
    [[nodiscard]] const CursorPosition& position() const noexcept;
    [[nodiscard]] bool exhausted() const noexcept;

private:
    ArchiveReader reader_;
    CursorPosition position_;
    bool exhausted_ = false;
};

[[nodiscard]] std::string render_cursor_position(const CursorPosition& position);

} // namespace aethon::storage
