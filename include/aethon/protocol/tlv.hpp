#pragma once

#include "aethon/common/bytes.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aethon::protocol {

struct TlvField {
    std::uint16_t type = 0;
    Bytes value;
};

struct TlvParseOptions {
    std::size_t max_fields = 128;
    std::size_t max_value_size = 64 * 1024;
    bool require_full_buffer = true;
};

std::vector<TlvField> parse_tlv_fields(std::span<const std::uint8_t> bytes, TlvParseOptions options = {});
Bytes encode_tlv_fields(std::span<const TlvField> fields);

std::optional<TlvField> find_tlv_field(std::span<const TlvField> fields, std::uint16_t type);
std::optional<std::uint64_t> tlv_unsigned(const TlvField& field);
std::optional<std::string> tlv_string(const TlvField& field, std::size_t max_size = 4096);

Bytes make_tlv_u64(std::uint16_t type, std::uint64_t value);
Bytes make_tlv_string(std::uint16_t type, std::string_view value);

} // namespace aethon::protocol
