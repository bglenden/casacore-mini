# P8-W11 Open Issues

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Blocking issues

None.

## Non-blocking issues

1. **IGRF not implemented** — The International Geomagnetic Reference Field
   model is not implemented in casacore-mini. MEarthMagnetic conversions use
   a simplified dipole model rather than the full IGRF coefficient tables.
   This is documented in known_differences.md and is acceptable for the
   Phase 8 scope since MEarthMagnetic is rarely used in practice.
