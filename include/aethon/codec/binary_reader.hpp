#pragma once

#include "aethon/common/bytes.hpp"
#include "aethon/common/error.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace aethon::codec {

class BinaryReader {
public:
    explicit BinaryReader(std::span<const std::uint8_t> data);
    [[nodiscard]] std::size_t remaining() const noexcept;
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] bool empty() const noexcept { return remaining() == 0; }
    std::uint8_t u8();
    std::uint16_t u16();
    std::uint32_t u32();
    std::uint64_t u64();
    Bytes bytes(std::size_t n);
    std::string string(std::size_t max_len = 4096);
private:
    void require(std::size_t n) const;
    std::span<const std::uint8_t> data_;
    std::size_t offset_ = 0;
};

} // namespace aethon::codec
