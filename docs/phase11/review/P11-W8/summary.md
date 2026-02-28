# P11-W8 Review: Integration Convergence

## Scope
Cross-tranche integration testing verifying TaQL queries, MSSelection
evaluation, and the to_taql_where bridge all work together over a
shared MeasurementSet.

## Deliverables
- `tests/phase11_integration_test.cpp` — 24 checks over a 30-row MS
  covering direct TaQL (SELECT, COUNT, CALC, SHOW), MSSelection evaluate,
  to_taql_where parity, combined MSSelection+TaQL injection, ROWNR in
  MS context, and API consistency (has_selection, clear, accessors).
- `tools/check_phase11_w8.sh` — gate script, 14/14 pass.

## Test Matrix
| Category           | Checks |
|--------------------|--------|
| TaQL SELECT WHERE  | 3      |
| TaQL COUNT         | 2      |
| TaQL CALC          | 1      |
| TaQL SHOW          | 1      |
| MSSelection eval   | 4      |
| to_taql_where      | 4      |
| Combined inject    | 3      |
| ROWNR in MS        | 2      |
| API consistency    | 4      |

## Result
Gate passes 14/14.
