#include "aethon/protocol/tlv.hpp"

#include "aethon/codec/binary_reader.hpp"
#include "aethon/codec/binary_writer.hpp"
#include "aethon/common/error.hpp"

#include <limits>
#include <utility>

namespace aethon::protocol {

std::vector<TlvField> parse_tlv_fields(std::span<const std::uint8_t> bytes, TlvParseOptions options) {
    codec::BinaryReader reader(bytes);
    std::vector<TlvField> fields;
    while (!reader.empty()) {
        if (reader.remaining() < 4) {
            if (options.require_full_buffer) {
                throw Error(ErrorCode::malformed_packet, "truncated tlv header");
            }
            break;
        }
        if (fields.size() >= options.max_fields) {
            throw Error(ErrorCode::malformed_packet, "too many tlv fields");
        }
        TlvField field;
        field.type = reader.u16();
        auto length = reader.u16();
        if (length > options.max_value_size) {
            throw Error(ErrorCode::malformed_packet, "tlv field exceeds maximum value size");
        }
        field.value = reader.bytes(length);
        fields.push_back(std::move(field));
    }
    return fields;
}

Bytes encode_tlv_fields(std::span<const TlvField> fields) {
    codec::BinaryWriter writer;
    for (const auto& field : fields) {
        if (field.value.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw Error(ErrorCode::invalid_argument, "tlv field is too large to encode");
        }
        writer.u16(field.type);
        writer.u16(static_cast<std::uint16_t>(field.value.size()));
        writer.bytes(field.value);
    }
    return writer.take();
}

std::optional<TlvField> find_tlv_field(std::span<const TlvField> fields, std::uint16_t type) {
    for (const auto& field : fields) {
        if (field.type == type) {
            return field;
        }
    }
    return std::nullopt;
}

std::optional<std::uint64_t> tlv_unsigned(const TlvField& field) {
    if (field.value.empty() || field.value.size() > 8) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < field.value.size(); ++i) {
        value |= static_cast<std::uint64_t>(field.value[i]) << (8 * i);
    }
    return value;
}

std::optional<std::string> tlv_string(const TlvField& field, std::size_t max_size) {
    if (field.value.size() > max_size) {
        return std::nullopt;
    }
    for (auto byte : field.value) {
        if (byte == 0) {
            return std::nullopt;
        }
    }
    return std::string(field.value.begin(), field.value.end());
}

Bytes make_tlv_u64(std::uint16_t type, std::uint64_t value) {
    TlvField field;
    field.type = type;
    do {
        field.value.push_back(static_cast<std::uint8_t>(value & 0xff));
        value >>= 8;
    } while (value != 0);
    return encode_tlv_fields(std::span<const TlvField>(&field, 1));
}

Bytes make_tlv_string(std::uint16_t type, std::string_view value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw Error(ErrorCode::invalid_argument, "tlv string is too large");
    }
    TlvField field;
    field.type = type;
    field.value.assign(value.begin(), value.end());
    return encode_tlv_fields(std::span<const TlvField>(&field, 1));
}

} // namespace aethon::protocol
