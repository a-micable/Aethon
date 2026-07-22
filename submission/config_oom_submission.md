# Submission: Unbounded Config Input Materialization Causes OOM

## Bug Name

Unbounded ConfigParser input materialization before line and entry limits

## Summary

`ConfigParser::parse_bytes` copies the entire attacker-controlled byte span into a `std::string` before any total input-size limit is enforced. The parser later enforces per-line and entry-count limits, but those checks happen after the full input has already been reserved and copied. A crafted config input can therefore force deterministic memory exhaustion in `config_fuzzer` before semantic parsing limits are reached.

## Impact

An attacker-controlled configuration blob can exhaust process memory and terminate the collector/config processing path. The crash is deterministic and does not require slow accumulation or many parser iterations; the allocation is triggered directly by the input size passed to `parse_bytes`.

## Affected Code

- `src/config/config_parser.cpp`
- `ConfigParser::parse_bytes`

The vulnerable sequence is:

```cpp
std::string text;
text.reserve(bytes.size());
```

There is no total cap on `bytes.size()` before the allocation.

## C++ PoC

Build the generator:

```sh
c++ -std=c++20 -O2 pocs/config_oom_poc.cpp -o /tmp/config_oom_poc
```

Generate the crashing config input:

```sh
/tmp/config_oom_poc poc-config-oom.bin
```

Run the target verifier/fuzzer against the generated file:

```sh
/out/config_fuzzer -- -rss_limit_mb=2560 -timeout=25 poc-config-oom.bin
```

Expected result: deterministic OOM in `config_fuzzer`.

## Patch

Apply `patches/config_total_input_cap.patch`.

The fix adds a total input cap to `ConfigParser` and rejects oversized configuration blobs before reserving or copying attacker-controlled input.

## Why This Patch

The existing `max_line_length_` and `max_entries_` limits are parser-stage limits. They do not protect the pre-parse materialization step in `parse_bytes`. A separate `max_input_bytes_` limit closes the allocation path directly and keeps the parser’s existing line and entry validation intact.
