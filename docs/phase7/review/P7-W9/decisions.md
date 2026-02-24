# P7-W9 Decisions

## Decision 1: Aggregate replaces individual calls in CI
**Choice**: Replaced the six individual `check_phase7_wN.sh` calls in the CI
script with a single `check_phase7.sh` call.

**Rationale**: Reduces maintenance burden and ensures the aggregate script is
the single source of truth for Phase 7 acceptance. Individual wave gate scripts
remain available for targeted debugging.

## Decision 2: W9 gate validates CI integration
**Choice**: The W9 gate verifies not just that `check_phase7.sh` passes, but
also that the CI script references it and does NOT still reference individual
wave checks.

**Rationale**: Prevents regression where someone adds a new wave check directly
to the CI script instead of to the aggregate. The gate enforces the pattern.
