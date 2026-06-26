# Fuzzing Guide

Aethon keeps every major binary and text input path independently fuzzable. The harnesses under `fuzz/` are intentionally small, but each one calls production code rather than duplicating parsers in the harness.

## Targets

- `packet_fuzzer.cpp`: full packet frame decode, payload transform, re-encode, strict decode.
- `archive_fuzzer.cpp`: `.ath` reader construction, record iteration, packet revalidation.
- `stream_fuzzer.cpp`: streaming parser resynchronization, split input delivery, error callbacks.
- `replay_fuzzer.cpp`: archive reader plus replay engine with routing summary callbacks.
- `config_fuzzer.cpp`: configuration parser, typed getters, and config loader summaries.
- `section_fuzzer.cpp`: optional protocol section decoder and extension registry state.
- `transform_fuzzer.cpp`: compression, decompression, envelope sealing/opening, digest cache.
- `state_fuzzer.cpp`: routing, query, telemetry, and pipeline state machines.

## Corpora And Dictionary

Seed inputs live under `fuzz/corpus/` by target family. They include protocol-like frames, optional section bodies, archive-like files, realistic collector configs, transform payloads, and state-machine event streams. `fuzz/aethon.dict` contains protocol magics, common config keys, packet kinds, and metadata tokens.

The corpora are not expected to be exhaustive. They are meant to get libFuzzer past empty-input checks and into nested packet sections, quoted config values, replay callbacks, and bounded state windows quickly.

## ClusterFuzzLite

ClusterFuzzLite builds with:

```sh
.clusterfuzzlite/build.sh
```

The build script copies all `*_fuzzer` binaries, dictionaries, and seed corpora into `$OUT`. The default build is deterministic from a clean checkout and does not fetch dependencies.

## Maintenance

When adding a new parser or stateful input boundary, add a fuzz target or extend an existing one in the same change. Do not add artificial crashes or intentionally vulnerable behavior; fuzzing should exercise production validation and error handling.
