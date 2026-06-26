#pragma once

#include "aethon/common/bytes.hpp"
#include <cstdint>
#include <span>
#include <string_view>

namespace aethon::codec {

class BinaryWriter {
public:
    void u8(std::uint8_t value);
    void u16(std::uint16_t value);
    void u32(std::uint32_t value);
    void u64(std::uint64_t value);
    void bytes(std::span<const std::uint8_t> value);
    void string(std::string_view value);
    [[nodiscard]] const Bytes& buffer() const noexcept { return buffer_; }
    [[nodiscard]] Bytes take() noexcept;
private:
    Bytes buffer_;
};

} // namespace aethon::codec
