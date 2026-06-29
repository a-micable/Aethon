# API Notes

Aethon exposes a small set of library entry points rather than a service framework.

## Packet Frames

Use `aethon::protocol::decode_packet` when a complete `ATHN` frame is available. The decoder verifies magic, length, protocol version, optional section encoding, payload length, and CRC32C before returning a `protocol::Packet`.

Use `aethon::protocol::encode_packet` to serialize a packet after constructing or modifying its metadata. The encoder writes the same frame format used by archives, fuzz seeds, and the command line tools.

## Streams

Use `aethon::stream::StreamParser` for unreliable byte streams. It buffers partial data, searches for packet magic after link noise, validates complete frames, and reports decoded packets through callbacks.

## Archives And Replay

Use `aethon::storage::ArchiveWriter` to create `.ath` files and `aethon::storage::ArchiveReader` to iterate validated records. `build_archive_index` creates a lightweight timestamp index for inspection tools.

Use `aethon::replay::ReplayEngine` when an archive needs to be replayed through caller-provided handlers. Replay options support time-window filtering and optional timing preservation.

## Configuration

Use `aethon::config::ConfigParser` for collector-style key/value files. It handles comments, quoted values, basic escapes, typed getters, maximum line lengths, and entry-count limits.
