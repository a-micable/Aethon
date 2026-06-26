# RollbackPlan

The `rollback_plan` component handles rollback steps and is used for rollback readiness. It keeps a bounded event window and exposes deterministic evaluation helpers so replay and live collection paths can compare behavior.

Operators should review decisions from this component alongside packet loss, clock skew, and collector region before changing production thresholds.
