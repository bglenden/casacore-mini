# P7-W11 Open Issues

## Blocking issues

None for W11 scope.

## Non-blocking issues

1. All 6 mini→casacore directory cells fail because mini does not yet produce
   SM data files (`table.f*`). This is expected and will be resolved by W12–W16.
2. OMP warnings (`Can't set size of /tmp file`) appear during casacore tool
   execution in sandboxed environments. These are cosmetic and do not affect
   correctness.

## Proposed follow-ups

1. W12: Implement StandardStMan read path to resolve table_dir casacore→mini
   cell-value verification.
2. W13: Implement StandardStMan write path to resolve table_dir mini→casacore
   cell.
