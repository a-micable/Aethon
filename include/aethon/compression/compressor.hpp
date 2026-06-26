#pragma once
#include "aethon/protocol/types.hpp"
#include <memory>
#include <span>
namespace aethon::compression {
class Compressor { public: virtual ~Compressor() = default; virtual protocol::CompressionMode mode() const noexcept = 0; virtual Bytes compress(std::span<const std::uint8_t> input) const = 0; virtual Bytes decompress(std::span<const std::uint8_t> input, std::size_t max_output) const = 0; };
std::unique_ptr<Compressor> make_compressor(protocol::CompressionMode mode);
} // namespace aethon::compression
