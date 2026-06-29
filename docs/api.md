# API Notes

Aethon exposes a small set of library entry points rather than a service framework.

## Packet Frames

Use `aethon::protocol::decode_packet` when a complete `ATHN` frame is available. The decoder verifies magic, length, protocol version, optional section encoding, payload length, and CRC32C before returning a `protocol::Packet`.

Use `aethon::protocol::encode_packet` to serialize a packet after constructing or modifying its metadata. The encoder writes the same frame format used by archives, fuzz seeds, and the command line tools.

Use `aethon::protocol::parse_tlv_fields` for extension payloads that contain nested metadata. TLV helpers decode bounded fields, extract unsigned values and strings, and re-encode validated field lists.

Use `aethon::protocol::inspect_packet` when tools need a stable, human-readable packet summary. The inspector reports version, kind, device id, routing metadata, fragment state, capabilities, extensions, and warnings for suspicious but decodable packets.

## Streams

Use `aethon::stream::StreamParser` for unreliable byte streams. It buffers partial data, searches for packet magic after link noise, validates complete frames, and reports decoded packets through callbacks.

Use `aethon::protocol::FragmentReassembler` when decoded packets carry fragment metadata. It tracks fragments by device and stream id, rejects inconsistent coordinates, ignores duplicates, and emits a completed packet only when all fragments have arrived.

## Archives And Replay

Use `aethon::storage::ArchiveWriter` to create `.ath` files and `aethon::storage::ArchiveReader` to iterate validated records. `build_archive_index` creates a lightweight timestamp index for inspection tools.

Use `aethon::storage::ArchiveRepairScanner` to scan damaged captures for embedded packet frames. It validates candidate frames with the production decoder and can write a repaired archive from the salvage report.

Use `aethon::replay::ReplayEngine` when an archive needs to be replayed through caller-provided handlers. Replay options support time-window filtering and optional timing preservation.

## Configuration

Use `aethon::config::ConfigParser` for collector-style key/value files. It handles comments, quoted values, basic escapes, typed getters, maximum line lengths, and entry-count limits.

Use `aethon::config::ConfigSchema` to validate required keys, booleans, integer ranges, and deployment-specific collector settings after parsing.
