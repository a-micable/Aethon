# SegmentWriter

The `segment_writer` component tracks capture segments and contributes rollover decisions. Operators use its summaries to distinguish routine telemetry changes from conditions that require replay or archive inspection.

## Operating Notes 1

Keep live windows bounded and prefer archive replay when investigating long-running behavior.
