# API Gap Analysis: casacore-original vs casacore-mini

This document catalogs API-level differences between casacore-original and
casacore-mini, discovered during demo program development. Each gap is
classified by severity and accompanied by a proposal for resolution.

## Summary

| Phase | Domain | Gap Count | Severity |
|-------|--------|-----------|----------|
| 1-2   | AipsIO + Record | 1 | Low |
| 4     | Keyword Records | 1 | Low |
| 7     | Tables | 4 | Medium-High |
| 8     | Measures | 2 | Low-Medium |
| 8     | Coordinates | 2 | Medium |
| 9     | MeasurementSet | 3 | Medium |

---

## Phase 1-2: AipsIO + Record

### G1: No operator<< / pretty-print for Record

**casacore-original:** `cout << rec` prints a human-readable representation.
**casacore-mini:** No `operator<<` or `to_string()` for `Record`.
**Severity:** Low
**Proposal:** Add `std::string format_record(const Record&)` utility.

---

## Phase 4: Keyword Records

### G2: KeywordRecord is separate from Record

**casacore-original:** Table keywords use the same `RecordInterface` hierarchy.
**casacore-mini:** `KeywordRecord` (showtableinfo parser output) is a separate
type from `Record` (AipsIO persistence model). Conversion requires
`keyword_record_convert.hpp`.
**Severity:** Low -- conversion functions exist and work correctly.
**Proposal:** Document the distinction. No code change needed since both types
serve different purposes (text parsing vs binary persistence).

---

## Phase 7: Tables

### G3: No add_row() / row count mutation (HIGH)

**casacore-original:** `Table::addRow(n)` adds rows dynamically.
**casacore-mini:** `Table::create()` requires row count at creation time. No
dynamic row addition after creation.
**Severity:** High
**Proposal:** Implement `Table::add_row()` that re-allocates SSM buckets. This
requires SsmWriter to support append mode, which is a non-trivial extension.
Current workaround: create with known row count.

### G4: No column addition / TableDesc mutation

**casacore-original:** `Table::addColumn(ColumnDesc)` adds columns to an
existing table.
**casacore-mini:** Column set is fixed at `Table::create()` time.
**Severity:** Medium
**Proposal:** Defer until needed. Most use cases create tables with a known
schema.

### G5: SSM-only for Table::create()

**casacore-original:** `SetupNewTable` can bind columns to any storage manager
(SSM, ISM, TSM, etc.).
**casacore-mini:** `Table::create()` always uses SSM for all columns.
**Severity:** Medium -- TSM/ISM read works via `Table::open()`.
**Proposal:** Add `TableColumnSpec::sm_type` field or builder pattern for
advanced SM bindings.

### G6: No table locking

**casacore-original:** `Table` supports read/write locking via `table.lock`.
**casacore-mini:** No locking support; concurrent access is not safe.
**Severity:** Low for single-user CLI workflows.
**Proposal:** Add advisory file locking if multi-process support is needed.

---

## Phase 8: Measures

### G7: No MeasFrame in Direction conversions (for APP/HADEC/AZEL)

**casacore-original:** `MDirection::Convert` with an `MeasFrame` containing
epoch + position for time-dependent frames (APP, HADEC, AZEL).
**casacore-mini:** `convert_measure()` accepts a `MeasureFrame` with epoch/
position, but only certain frame combinations are fully implemented.
**Severity:** Low-Medium -- J2000/Galactic/Ecliptic/B1950 work fully.
**Proposal:** Extend `convert_measure()` for APP/HADEC/AZEL frames.

### G8: No Frequency measure conversions

**casacore-original:** `MFrequency::Convert` between REST/LSRK/TOPO etc.
**casacore-mini:** `FrequencyRef` enum exists but `convert_measure()` does not
support frequency-frame conversions.
**Severity:** Medium for spectral line work.
**Proposal:** Implement frequency conversions using Doppler shift + velocity.

---

## Phase 8: Coordinates

### G9: No composite toWorld()/toPixel() on CoordinateSystem (MEDIUM)

**casacore-original:** `CoordinateSystem::toWorld(worldOut, pixelIn)` converts a
full pixel vector (across all axes) to world coordinates in one call.
**casacore-mini:** Only per-coordinate `cs.coordinate(i).to_world()` is
available. Users must manually split pixel vectors by coordinate and reassemble.
**Severity:** Medium
**Proposal:** Add `CoordinateSystem::to_world(pixel)` and
`CoordinateSystem::to_pixel(world)` that dispatch to each sub-coordinate.

### G10: No LinearCoordinate / TabularCoordinate in demo

**casacore-original:** These coordinate types exist and are used in some FITS
images.
**casacore-mini:** `LinearCoordinate` and `TabularCoordinate` are implemented
but not exercised in demos. They work but are less commonly needed.
**Severity:** Low
**Proposal:** No action needed; existing unit tests cover these.

---

## Phase 9: MeasurementSet

### G11: MsSelection returns row indices, not filtered Table view (MEDIUM)

**casacore-original:** `MSSelection::toTableExprNode()` returns a `TableExprNode`
that filters the Table -- subsequent column reads automatically apply the filter.
**casacore-mini:** `MsSelection::evaluate()` returns `MsSelectionResult` with a
`rows` vector of selected row indices. Users must manually index into the
original table to read data for selected rows.
**Severity:** Medium -- functional but less ergonomic.
**Proposal:** This is a fundamental architectural difference. casacore-mini's
approach is explicit and works well with the current Table abstraction. Consider
adding a `FilteredTable` or `TableView` wrapper if demand arises.

### G12: No DATA column read in MsMainColumns

**casacore-original:** `MSMainColumns::data()` returns complex visibility data.
**casacore-mini:** `MsMainColumns` reads scalar columns and UVW/sigma/weight
arrays, but does not expose DATA/CORRECTED_DATA/MODEL_DATA complex array
columns.
**Severity:** Medium-High for data processing pipelines.
**Proposal:** Add `MsMainColumns::data()` returning
`std::vector<std::complex<float>>` via TSM read.

### G13: MsAntennaColumns::position() returns empty

**casacore-original:** `MSAntennaColumns::position()` reads the ITRF XYZ array.
**casacore-mini:** Returns empty vector (SSM array read for subtable arrays not
yet implemented).
**Severity:** Medium
**Proposal:** Fix SSM reader to support fixed-shape array reads in subtables,
or route through the new `Table` + `ArrayColumn<double>` abstraction.

---

## Priority Ranking

1. **G3** (add_row) -- High impact, needed for dynamic table building
2. **G12** (DATA column) -- High impact for data processing
3. **G9** (composite toWorld) -- Medium, improves coordinate usability
4. **G11** (selection as table view) -- Medium, ergonomic improvement
5. **G13** (antenna position) -- Medium, needed for UVW computation
6. **G5** (multi-SM create) -- Medium, needed for large tables with TSM
7. **G8** (frequency conversion) -- Medium, needed for spectral line
8. **G7** (APP/HADEC direction) -- Low-Medium
9. **G4** (add column) -- Medium, rare use case
10. **G1** (Record print) -- Low
11. **G2** (KeywordRecord) -- Low, documented distinction
12. **G6** (locking) -- Low
13. **G10** (LinearCoord demo) -- Low, already tested
