#include "aethon/common/bytes.hpp"

namespace aethon {

std::string to_hex(std::span<const std::uint8_t> bytes, std::size_t limit) {
    std::string out;
    const auto count = bytes.size() < limit ? bytes.size() : limit;
    out.reserve(count * 2 + (bytes.size() > count ? 3 : 0));
    for (std::size_t i = 0; i < count; ++i) {
        out += hex_byte(bytes[i]);
    }
    if (bytes.size() > count) {
        out += "...";
    }
    return out;
}

} // namespace aethon
