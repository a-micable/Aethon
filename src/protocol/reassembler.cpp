#include "aethon/protocol/reassembler.hpp"

#include "aethon/common/error.hpp"

#include <utility>

namespace aethon::protocol {

FragmentReassembler::FragmentReassembler(std::size_t max_payload_bytes)
    : max_payload_bytes_(max_payload_bytes) {}

std::optional<Packet> FragmentReassembler::push(Packet fragment) {
    ++stats_.fragments_seen;
    if (!fragment.fragment) {
        return fragment;
    }

    const auto info = *fragment.fragment;
    if (info.count == 0 || info.sequence >= info.count) {
        ++stats_.rejected_fragments;
        throw Error(ErrorCode::malformed_packet, "invalid fragment coordinates");
    }

    ReassemblyKey key{fragment.device, info.stream_id};
    auto [it, inserted] = pending_.try_emplace(key);
    auto& pending = it->second;
    if (inserted) {
        pending.first_packet = fragment;
        pending.first_packet.payload.clear();
        pending.first_packet.fragment.reset();
        pending.fragments.resize(info.count);
        pending.present.assign(info.count, false);
    } else if (pending.fragments.size() != info.count) {
        ++stats_.rejected_fragments;
        throw Error(ErrorCode::malformed_packet, "fragment count changed within stream");
    }

    if (pending.present[info.sequence]) {
        ++stats_.duplicate_fragments;
        return std::nullopt;
    }

    if (pending.total_payload + fragment.payload.size() > max_payload_bytes_) {
        pending_.erase(it);
        ++stats_.rejected_fragments;
        throw Error(ErrorCode::malformed_packet, "reassembled payload exceeds configured limit");
    }

    pending.total_payload += fragment.payload.size();
    pending.fragments[info.sequence] = std::move(fragment.payload);
    pending.present[info.sequence] = true;
    ++pending.received;

    if (pending.received != pending.fragments.size()) {
        stats_.pending_streams = pending_.size();
        return std::nullopt;
    }

    Packet assembled = std::move(pending.first_packet);
    assembled.payload.reserve(pending.total_payload);
    for (const auto& part : pending.fragments) {
        assembled.payload.insert(assembled.payload.end(), part.begin(), part.end());
    }
    pending_.erase(it);
    ++stats_.completed_messages;
    stats_.pending_streams = pending_.size();
    return assembled;
}

void FragmentReassembler::clear(DeviceId device, std::uint32_t stream_id) {
    pending_.erase(ReassemblyKey{device, stream_id});
    stats_.pending_streams = pending_.size();
}

void FragmentReassembler::clear_all() {
    pending_.clear();
    stats_.pending_streams = 0;
}

ReassemblyStats FragmentReassembler::stats() const {
    auto copy = stats_;
    copy.pending_streams = pending_.size();
    return copy;
}

} // namespace aethon::protocol
