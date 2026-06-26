# API Notes

Aethon separates packet validation, archive processing, routing, diagnostics, and replay so operational tools can share the same core behavior.

## Design Notes

The module boundaries are intentionally narrow. Parsers return validated packet objects. Storage preserves capture order and integrity metadata. Replay and routing apply policy outside the parser.

## Maintenance

Changes should keep public headers stable, add focused tests for edge cases, and avoid coupling deployment-specific behavior into the protocol layer.
`aethon::analysis::BurstClassifier` exposes bounded observation, summary, latest-sample lookup, and threshold selection helpers for collector-side processing.
`aethon::analysis::InterferenceMap` exposes bounded observation, summary, latest-sample lookup, and threshold selection helpers for collector-side processing.
`aethon::stream::JitterBuffer` exposes bounded observation, summary, latest-sample lookup, and threshold selection helpers for collector-side processing.
`aethon::storage::ArchiveIndex` exposes bounded observation, summary, latest-sample lookup, and threshold selection helpers for collector-side processing.
`aethon::storage::ArchiveCompaction` exposes bounded observation, summary, latest-sample lookup, and threshold selection helpers for collector-side processing.
