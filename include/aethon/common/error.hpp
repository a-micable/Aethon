#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace aethon {

enum class ErrorCode {
    eof,
    invalid_argument,
    malformed_packet,
    checksum_mismatch,
    unsupported_version,
    archive_corrupt,
    routing_failed,
    plugin_error,
    invariant_violation,
};

class Error : public std::runtime_error {
public:
    Error(ErrorCode code, std::string message)
        : std::runtime_error(std::move(message)), code_(code) {}

    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

} // namespace aethon
