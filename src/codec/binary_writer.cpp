#include "aethon/codec/binary_writer.hpp"
#include "aethon/common/error.hpp"

namespace aethon::codec {

void BinaryWriter::u8(std::uint8_t value) { buffer_.push_back(value); }

void BinaryWriter::u16(std::uint16_t value) {
    buffer_.push_back(static_cast<std::uint8_t>(value & 0xff));
    buffer_.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

void BinaryWriter::u32(std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        buffer_.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xff));
    }
}

void BinaryWriter::u64(std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buffer_.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xff));
    }
}

void BinaryWriter::bytes(std::span<const std::uint8_t> value) {
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void BinaryWriter::string(std::string_view value) {
    if (value.size() > 65535) {
        throw Error(ErrorCode::invalid_argument, "wire string is too large");
    }
    u16(static_cast<std::uint16_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

Bytes BinaryWriter::take() noexcept {
    Bytes out;
    out.swap(buffer_);
    return out;
}

} // namespace aethon::codec
