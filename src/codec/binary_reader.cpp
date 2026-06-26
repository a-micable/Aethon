#include "aethon/codec/binary_reader.hpp"

namespace aethon::codec {

BinaryReader::BinaryReader(std::span<const std::uint8_t> data) : data_(data) {}

std::size_t BinaryReader::remaining() const noexcept {
    return data_.size() - offset_;
}

void BinaryReader::require(std::size_t n) const {
    if (remaining() < n) {
        throw Error(ErrorCode::eof, "binary reader reached end of input");
    }
}

std::uint8_t BinaryReader::u8() {
    require(1);
    return data_[offset_++];
}

std::uint16_t BinaryReader::u16() {
    require(2);
    auto v = static_cast<std::uint16_t>(data_[offset_])
        | static_cast<std::uint16_t>(data_[offset_ + 1] << 8);
    offset_ += 2;
    return v;
}

std::uint32_t BinaryReader::u32() {
    require(4);
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(data_[offset_ + i]) << (8 * i);
    }
    offset_ += 4;
    return v;
}

std::uint64_t BinaryReader::u64() {
    require(8);
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(data_[offset_ + i]) << (8 * i);
    }
    offset_ += 8;
    return v;
}

Bytes BinaryReader::bytes(std::size_t n) {
    require(n);
    Bytes out(data_.begin() + static_cast<std::ptrdiff_t>(offset_),
              data_.begin() + static_cast<std::ptrdiff_t>(offset_ + n));
    offset_ += n;
    return out;
}

std::string BinaryReader::string(std::size_t max_len) {
    auto n = u16();
    if (n > max_len) {
        throw Error(ErrorCode::malformed_packet, "string exceeds configured limit");
    }
    auto raw = bytes(n);
    return std::string(raw.begin(), raw.end());
}

} // namespace aethon::codec
