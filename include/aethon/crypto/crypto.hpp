#pragma once
#include "aethon/protocol/types.hpp"
#include <memory>
#include <span>
namespace aethon::crypto {
struct KeyMaterial { Bytes bytes; std::uint32_t key_id = 0; };
class Cipher { public: virtual ~Cipher() = default; virtual protocol::EncryptionMode mode() const noexcept = 0; virtual Bytes seal(std::span<const std::uint8_t> plaintext, const KeyMaterial& key) const = 0; virtual Bytes open(std::span<const std::uint8_t> ciphertext, const KeyMaterial& key) const = 0; };
std::unique_ptr<Cipher> make_cipher(protocol::EncryptionMode mode);
} // namespace aethon::crypto
