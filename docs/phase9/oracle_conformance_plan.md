# Oracle Conformance Test Plan (Phase 9 Closeout)

## Context

The Phase 9 interop matrix currently verifies MS operations via independent
produce/verify subcommands in two tools, but the cross-tool cells have 7
failures due to table-format gaps. A stronger gate is needed: an authoritative
dump of every value in a real MS, generated once by upstream casacore, that
casacore-mini then verifies cell-by-cell. This removes ambiguity — if
casacore-mini reads a different value from the same on-disk bytes, it's a bug.

The test MS is `mssel_test_small_multifield_spw.ms.tgz` (4.8 MB, real
multi-field/multi-spw data with 15 subtables including optional ones).

## Approach

Two new subcommands added to the existing interop tools (not new programs):

1. **`dump-oracle-ms`** in `casacore_interop_tool.cpp` — opens the test MS
   via upstream casacore, dumps structure + every keyword + every cell value
   to a deterministic line-based text file.

2. **`verify-oracle-ms`** in `interop_mini_tool.cpp` — parses the oracle
   dump, opens the same MS via casacore-mini, verifies everything matches.
   Skips are allowed only for items in `known_differences.md` (optional
   subtables DOPPLER/SOURCE/SYSCAL, FLAG_CATEGORY).

The oracle dump is generated once and committed to the repo. A regeneration
script exists for refreshing it when needed.

## Dump Format (line-based text)

Deterministic, diff-friendly. Tables sorted: MAIN first, then subtables
alphabetically. Within each table: column descriptors in file order, keywords
alphabetically, cell values by column name (alpha) then row number.

```
oracle_version=1
source_ms=mssel_test_small_multifield_spw.ms
table_count=<N>

table=<NAME>
table=<NAME>.row_count=<N>
table=<NAME>.ncol=<N>
table=<NAME>.col[<i>].name_b64=<base64>
table=<NAME>.col[<i>].kind=scalar|array
table=<NAME>.col[<i>].dtype=TpInt|TpFloat|TpDouble|TpComplex|TpString|TpBool|...
table=<NAME>.col[<i>].ndim=<N>
table=<NAME>.col[<i>].shape=<d0,d1,...>        (only if fixed shape)
table=<NAME>.col[<i>].options=<int>
table=<NAME>.col[<i>].dm_type_b64=<base64>
table=<NAME>.col[<i>].dm_group_b64=<base64>
table=<NAME>.kw.<path>=<type>|<value>
table=<NAME>.col_kw[<col_b64>].<path>=<type>|<value>
table=<NAME>.cell[<col_b64>][<row>]=<type>|<value>
```

Value encoding:
- Integers: decimal
- Floats/doubles: `std::hexfloat` (exact IEEE 754 round-trip)
- Complex: `(<real_hex>;<imag_hex>)`
- Strings: `b64:<base64>`
- Bools: `true`/`false`
- Arrays: `shape=d0,d1,...|values=v0,v1,...`
- Undefined cells: `undefined`
- Subtable references: `table_ref|b64:<path>`

## Implementation Steps

### Step 1: Format spec doc
- Create `docs/phase9/oracle_format.md` documenting the exact syntax.

### Step 2: Implement dumper (`dump-oracle-ms`)
- **File:** `tools/interop/casacore_interop_tool.cpp` (~400-500 new lines)
- Reuses existing hex-float, base64, record-line encoding helpers.
- Walks `Table::keywordSet()` for TpTable entries to discover subtables.
- Uses `ScalarColumn<T>` / `ArrayColumn<T>` for cell reads.
- Checks `isDefined()` for each array cell (subtables may have undefined cells).
- Emits all column descriptors, keywords, and cells per the format spec.

### Step 3: Generate and commit the oracle dump
- Create `tools/interop/generate_oracle.sh` (builds interop tool, extracts
  test MS, runs dumper, writes to `data/corpus/oracle/`).
- Run it, inspect output for sanity (correct table count, spot-check values).
- Commit `data/corpus/oracle/mssel_multifield_spw_oracle.txt`.

### Step 4: Implement verifier (`verify-oracle-ms`)
- **File:** `src/interop_mini_tool.cpp` (~500-600 new lines)
- Parses oracle dump into structured sections (map of table name → column
  descs + keywords + cells).
- For each oracle table:
  - If subtable not present in casacore-mini (optional): skip with warning.
  - Verify column descriptors match (count, names, types, ndim, shape,
    options, SM type/group).
  - Verify table and column keywords match.
  - For each cell: read via `Table::open()` and the `Table` cell-read APIs
    (`read_scalar_cell`, `read_array_*_cell`), compare against oracle value.
- **CRITICAL: no direct SM access.** The verifier MUST NOT directly
  instantiate `SsmReader`, `IsmReader`, or `TiledStManReader`. All cell
  access goes through the `Table` abstraction. If `Table` lacks support for
  a cell type (e.g. string arrays, indirect arrays of certain dtypes), that
  is a `Table` bug to be fixed — not a reason to bypass the abstraction.
  The previous implementation used hand-rolled SM dispatch and crashed (Bus
  error) on real-world MS data as a direct consequence.
- **Tolerance:** hex-exact preferred; tolerance-mode fallback per
  `docs/phase9/tolerances.md` (floats: abs 1e-5, doubles: abs 1e-10).
  Tolerance matches reported as warnings.
- **Skip allow-list:** DOPPLER, SOURCE, SYSCAL subtables; FLAG_CATEGORY column.
  Any unexpected skip is a failure.

### Step 5: Phase gate script
- Create `tools/check_oracle.sh`: extracts test MS, builds mini tool, runs
  `verify-oracle-ms`, exits non-zero on any failure.
- Add oracle check to `tools/check_phase9.sh`.

### Step 6: Fix any mismatches found
- The verifier will likely surface bugs in casacore-mini's readers for the
  real-world MS data. Fix them iteratively until the gate passes.

## Key Files

| File | Action |
|------|--------|
| `tools/interop/casacore_interop_tool.cpp` | Add `dump-oracle-ms` subcommand |
| `src/interop_mini_tool.cpp` | Add `verify-oracle-ms` subcommand |
| `data/corpus/oracle/mssel_multifield_spw_oracle.txt` | New — committed oracle dump |
| `tools/interop/generate_oracle.sh` | New — regeneration script |
| `tools/check_oracle.sh` | New — phase gate script |
| `docs/phase9/oracle_format.md` | New — format specification |

## Verification

1. Run `tools/interop/generate_oracle.sh` — produces the oracle dump.
2. Run `tools/check_oracle.sh` — verifier reads dump + MS, reports per-cell
   pass/fail/skip. Exit 0 = gate passes.
3. Inspect skip list — only expected items (optional subtables, FLAG_CATEGORY).
4. Any failures trigger debugging in casacore-mini reader code until fixed.

## Anticipated Challenges

- **Indirect array columns:** Subtable columns (FEED, POLARIZATION, etc.)
  use `.f0i` indirect storage. casacore-mini supports `read_indirect_float/double`
  but not all element types. Unsupported indirect types → skip with warning.
- **TSM variants:** Main table DATA/FLAG likely use TiledShapeStMan. casacore-mini's
  `TiledStManReader` must handle the specific tile layout in this MS.
- **Generic column→SM routing:** The verifier needs to read arbitrary columns
  in any subtable, not just the known MS columns. Requires generic SM dispatch
  based on `TableDatFull` metadata.
