# DrainPlan

The `drain_plan` component handles shutdown drains and is used for orderly drain planning. It keeps a bounded event window and exposes deterministic evaluation helpers so replay and live collection paths can compare behavior.

Operators should review decisions from this component alongside packet loss, clock skew, and collector region before changing production thresholds.
