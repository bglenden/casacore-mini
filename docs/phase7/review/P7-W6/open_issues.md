# P7-W6 Open Issues

## Blocking issues

None for W6 scope.

## Non-blocking issues

1. mini->casacore directory verification fails because mini does not produce SM
   data files (`table.f*`). This will be resolved by W12-W13 (StandardStMan
   read/write paths).
2. The table directory layer copies SM data files verbatim without interpreting
   bucket contents. Cell-level verification relies on casacore Table API.

## Proposed follow-ups

1. W12: Implement StandardStMan read path for cell-value verification.
2. W13: Implement StandardStMan write path to produce SM data files.
