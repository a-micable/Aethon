#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace aethon::fuzz {

inline std::filesystem::path temp_input_path(std::string_view prefix, const std::uint8_t* data, std::size_t size) {
    static std::atomic<std::uint64_t> counter{0};
    std::uint64_t digest = 1469598103934665603ull;
    for (std::size_t i = 0; i < size; ++i) {
        digest ^= data[i];
        digest *= 1099511628211ull;
    }
    auto tick = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto serial = counter.fetch_add(1, std::memory_order_relaxed);
    auto name = std::string(prefix) + "-" + std::to_string(tick) + "-" +
        std::to_string(serial) + "-" + std::to_string(digest) + ".bin";
    return std::filesystem::temp_directory_path() / name;
}

inline std::filesystem::path write_temp_input(std::string_view prefix, const std::uint8_t* data, std::size_t size) {
    auto path = temp_input_path(prefix, data, size);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return path;
}

} // namespace aethon::fuzz
