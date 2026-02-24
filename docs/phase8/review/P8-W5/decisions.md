# P8-W5 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **Keyword-based measure metadata encoding matching casacore convention** —
   TableMeasureDesc writes measure type and reference frame information into
   column keywords using the same `MEASINFO` record structure as casacore.
   This ensures that tables written by casacore-mini can be read by casacore
   and vice versa.

## Tradeoffs accepted

1. Keyword-based encoding means measure metadata is not visible in the table
   schema — it is stored as column keyword records. This matches casacore's
   approach and is necessary for interoperability, even though a schema-level
   representation would be cleaner for a greenfield design.
