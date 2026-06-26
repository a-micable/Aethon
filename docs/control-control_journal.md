# ControlJournal

The `control_journal` component handles control events and is used for journal summaries. It keeps a bounded event window and exposes deterministic evaluation helpers so replay and live collection paths can compare behavior.

Operators should review decisions from this component alongside packet loss, clock skew, and collector region before changing production thresholds.
