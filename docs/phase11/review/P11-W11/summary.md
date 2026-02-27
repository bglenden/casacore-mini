# P11-W11 Review: Hardening + Stress + Docs Convergence

## Scope
Final hardening pass: malformed input robustness, documentation
completeness, API doc annotations, and full regression suite.

## Deliverables
- `tools/check_phase11_w11.sh` — gate script, 14/14 pass.
- `docs/phase11/known_differences.md` — 22 documented differences (D1-D22).
- Doxygen `///` annotations in `taql.hpp` and `ms_selection.hpp`.
- All 9 test suites pass full regression.

## Hardening Evidence
| Check                        | Result |
|------------------------------|--------|
| Malformed input test         | PASS   |
| known_differences >= 10 items| PASS (22) |
| taql.hpp Doxygen present     | PASS   |
| ms_selection.hpp Doxygen     | PASS   |
| Full 9-suite regression      | 9/9 PASS |

## Result
Gate passes 14/14.
