# P8-W11 Summary

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Scope implemented

- Malformed-input test suites for measures (`measure_malformed_test.cpp`) and
  coordinates (`coordinate_malformed_test.cpp`) exercising error paths with
  invalid records, out-of-range values, and missing fields.
- Reference data absence test (`reference_data_absence_test.cpp`) verifying
  graceful degradation when IERS/leap-second data files are missing.
- Known differences document (`known_differences.md`) cataloging documented
  precision and behavioral differences between casacore and casacore-mini.

## Public API changes

None (testing and documentation only).

## Behavioral changes

- Explicit malformed-input guards ensure that invalid records throw
  descriptive exceptions rather than producing undefined behavior.
- Missing reference data produces `MissingReferenceDataError` rather than
  silent fallback or crash.

## Deferred items

None.
