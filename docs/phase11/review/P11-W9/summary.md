# P11-W9 Review: Phase-11 Interoperability Matrix

## Scope
Verify all mini->mini interoperability cells: TaQL artifact families
(select, calc/count/show, extended functions) and MSSelection artifact
families (antenna/field/spw, observation/feed/poln, time/uvdist/state,
TaQL bridge) all interoperate within the casacore-mini toolchain.

## Deliverables
- `tools/check_phase11_w9.sh` — gate script, 9/9 pass.
- Cross-toolchain cells (casa->mini, mini->casa) documented as pending
  in `docs/phase11/known_differences.md`.

## Matrix Coverage
| Cell                          | Status |
|-------------------------------|--------|
| mini->mini TaQL select        | PASS   |
| mini->mini TaQL calc/count    | PASS   |
| mini->mini TaQL extended func | PASS   |
| mini->mini MSSel ant/fld/spw  | PASS   |
| mini->mini MSSel obs/feed/pol | PASS   |
| mini->mini MSSel time/uv/state| PASS   |
| mini->mini MSSel-TaQL bridge  | PASS   |
| mini->mini cross-family       | PASS   |
| casa->mini / mini->casa       | Documented pending |

## Result
Gate passes 9/9.
