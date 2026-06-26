#pragma once
#include "aethon/common/error.hpp"
#include "aethon/protocol/packet.hpp"
#include <deque>
#include <functional>
#include <span>
#include <utility>
namespace aethon::stream {
class StreamParser { public: using PacketHandler = std::function<void(protocol::Packet)>; using ErrorHandler = std::function<void(const Error&)>; explicit StreamParser(protocol::DecodeOptions options = {}); void on_packet(PacketHandler handler) { packet_handler_ = std::move(handler); } void on_error(ErrorHandler handler) { error_handler_ = std::move(handler); } void push(std::span<const std::uint8_t> data); [[nodiscard]] std::size_t buffered() const noexcept { return buffer_.size(); } void clear(); private: bool try_extract_one(); void discard_until_magic(); protocol::DecodeOptions options_; std::deque<std::uint8_t> buffer_; PacketHandler packet_handler_; ErrorHandler error_handler_; };
} // namespace aethon::stream
