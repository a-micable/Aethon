# WatermarkTracker

The `watermark_tracker` component tracks event time and contributes late packet tracking. Operators use its summaries to distinguish routine telemetry changes from conditions that require replay or archive inspection.

## Operating Notes 1

Keep live windows bounded and prefer archive replay when investigating long-running behavior.
