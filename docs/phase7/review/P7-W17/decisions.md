# P7-W17 Decisions

## Autonomous decisions made

1. Wave gate chain runs W11 through W16 gates sequentially. Failure in any
   gate aborts the chain.
2. Matrix enforcement is strict: any failed cell fails the wave.

## Assumptions adopted

1. All 6 required storage managers are verified through the 24-cell matrix
   (6 cases x 4 directions).

## Tradeoffs accepted

1. Negative-path testing deferred to Phase 8. W17 focuses on positive-path
   completeness.
