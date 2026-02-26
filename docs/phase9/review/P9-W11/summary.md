# P9-W11 Summary: Interop matrix + hardening

## Deliverables

1. **ms_malformed_test.cpp** — 7 hardening tests covering:
   - Open non-existent path throws
   - Open empty directory throws
   - Open truncated table.dat throws
   - Create over existing path throws
   - Access missing subtable throws
   - Metadata on valid MS succeeds
   - Selection on empty subtable works

2. **interop_mini_tool.cpp** — 10 new commands (5 produce + 5 verify):
   - `produce-ms-minimal` / `verify-ms-minimal`
   - `produce-ms-representative` / `verify-ms-representative`
   - `produce-ms-optional-subtables` / `verify-ms-optional-subtables`
   - `produce-ms-concat` / `verify-ms-concat`
   - `produce-ms-selection-stress` / `verify-ms-selection-stress`

3. **casacore_interop_tool.cpp** — Matching 10 commands for casacore-side
   produce/verify using upstream casacore MS API.

4. **run_phase9_matrix.sh** — Already present from W1 skeleton; verified
   compatible with new MS artifact names.

## Test results

- 57/57 tests pass (ms_malformed_test added as test #57)
- Mini→mini round-trip passes for all 5 artifacts
- W11 gate script passes
