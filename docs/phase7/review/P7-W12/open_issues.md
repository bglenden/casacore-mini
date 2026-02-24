# P7-W12 Open Issues

## Blocking issues

None for W12 scope.

## Non-blocking issues

1. SSM write path not yet implemented; mini->casacore cell verification for
   SSM-backed tables will fail until W13.
2. SsmReader supports scalar and fixed-shape array columns only. Variable-shape
   arrays are not required by current corpus.

## Proposed follow-ups

1. W13: Implement SsmWriter to produce SSM data files readable by casacore.
