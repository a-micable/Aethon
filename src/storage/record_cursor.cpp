#include "aethon/storage/record_cursor.hpp"

#include <sstream>
#include <utility>

namespace aethon::storage {

RecordCursor::RecordCursor(std::filesystem::path path)
    : reader_(std::move(path)) {}

std::optional<ArchiveRecord> RecordCursor::next() {
    if (exhausted_) {
        return std::nullopt;
    }
    auto record = reader_.next();
    if (!record) {
        exhausted_ = true;
        return std::nullopt;
    }
    ++position_.records_seen;
    position_.last_capture_time_ns = record->capture_time_ns;
    position_.last_sequence = record->packet.sequence;
    return record;
}

const CursorPosition& RecordCursor::position() const noexcept {
    return position_;
}

bool RecordCursor::exhausted() const noexcept {
    return exhausted_;
}

std::string render_cursor_position(const CursorPosition& position) {
    std::ostringstream out;
    out << "record_cursor\n"
        << "  records_seen: "
        << position.records_seen
        << "\n"
        << "  last_capture_time_ns: "
        << position.last_capture_time_ns
        << "\n"
        << "  last_sequence: "
        << position.last_sequence
        << "\n";
    return out.str();
}

} // namespace aethon::storage
