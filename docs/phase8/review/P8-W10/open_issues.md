# P8-W10 Open Issues

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Blocking issues

None.

## Non-blocking issues

1. **Coordinate artifacts use semantic verification** — Unlike measure artifacts
   which can be compared via exact record equality, coordinate artifacts are
   verified semantically by checking that round-trip pixel<->world transforms
   agree within documented tolerances. This is necessary because coordinate
   record representations may differ in optional fields between casacore and
   casacore-mini while being functionally equivalent.
