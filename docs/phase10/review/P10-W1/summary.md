# P10-W1 Summary: API/corpus/LEL contract freeze

## Deliverables

1. `docs/phase10/api_surface_map.csv` — 65 API rows covering lattice core,
   traversal, LEL, image core, image metadata, image utilities, LC regions,
   and WC regions.
2. `docs/phase10/interop_artifact_inventory.md` — 5 required artifacts
   (img-minimal, img-cube-masked, img-region-subset, img-expression,
   img-complex) with semantic check definitions.
3. `docs/phase10/lel_coverage_contract.md` — 8 required LEL expression
   categories with parse/eval/negative test requirements.
4. `docs/phase10/tolerances.md` — pixel, expression, coordinate, shape/mask
   tolerances inheriting Phase 9 base.
5. `docs/phase10/dependency_decisions.md` — locked decisions for mdspan
   storage, hand-written LEL parser, tree-walk evaluator, table-based
   image persistence.
6. `docs/phase10/lint_profile.md` — inherited Phase 9 profile, locked.
7. `tools/check_phase10.sh` — aggregate Phase 10 gate runner.
8. `tools/check_phase10_w1.sh` — W1-specific gate (passes).
9. `tools/check_phase10_w{2..12}.sh` — skeleton wave checks for later waves.
10. `tools/interop/run_phase10_matrix.sh` — Phase 10 interop matrix runner.

## Gate result

`bash tools/check_phase10_w1.sh` — PASSED
