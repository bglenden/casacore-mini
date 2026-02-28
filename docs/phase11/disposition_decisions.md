# Phase 11 Disposition Decisions

Date: 2026-02-27

## Summary

Phase 11 is the terminal closure phase. The audit identified remaining upstream
capabilities and assigned each an explicit Include or Exclude disposition.

## Included capabilities (mandatory for phase closeout)

### 1. TaQL full command + expression surface (C11-T1)

**Rationale**: TaQL is the universal query interface for casacore tables.
Without it, users cannot filter, aggregate, or transform table data using the
standard query language. Every casacore-based tool pipeline uses TaQL
expressions.

**Scope**: All 10 command families (SELECT, UPDATE, INSERT, DELETE, COUNT,
CALC, CREATE TABLE, ALTER TABLE, DROP TABLE, SHOW), full expression grammar,
220+ built-in functions, aggregate functions, type system, unit support.

**Target waves**: P11-W3 (parser/AST), P11-W4 (evaluator/execution)

### 2. MSSelection full category surface (C11-T2)

**Rationale**: MSSelection is the standard row-selection API for
MeasurementSets. All CASA tasks and tools use it. The existing mini
implementation covers only basic integer parsing for 8 categories; the
upstream supports 12 categories with rich syntax (ranges, names, patterns,
baselines, frequency specs, time ranges, etc.).

**Scope**: 12 category parsers (antenna, field, spw/channel, scan, array,
time, uvdist, correlation/poln, state, observation, feed, raw TaQL), all
result accessors, error handling.

**Target waves**: P11-W5 (parsers), P11-W6 (evaluator/bridge)

### 3. MSSelection toTableExprNode bridge (C11-T3)

**Rationale**: This is the mechanism that converts MS selection expressions
into TaQL-compatible filter nodes for active query paths. Without it,
MSSelection results are disconnected from the query engine.

**Scope**: `to_table_expr_node()` method producing a TableExprNode tree that
can filter rows in concert with TaQL WHERE clauses.

**Target wave**: P11-W6

### 4. MeasUDF (TaQL measure UDFs)

**Rationale**: Measure-aware UDFs allow TaQL queries to perform coordinate
conversions inline (e.g., `MEAS.AZEL(...)` in a SELECT). This is a commonly
used capability in radio astronomy workflows.

**Scope**: Registration framework + core measure UDF families (epoch,
direction, position, frequency, radial velocity, doppler, earth magnetic
field). Implemented through TaQL's UDF extension point.

**Target wave**: P11-W4

### 5. ISM + TSM writer integration into Table::create()

**Rationale**: The plan requires that `casacore-mini` can produce tables with
all 6 required storage managers, not only StandardStMan. Currently
`Table::create()` routes to SSM by default; ISM and TSM writer paths exist
but are not fully integrated into the create API.

**Scope**: Wire ISM and all 4 TSM variants (TiledColumnStMan,
TiledCellStMan, TiledShapeStMan, TiledDataStMan) into `Table::create()` via
`TableCreateOptions::sm_type`.

**Target wave**: P11-W4

## Excluded capabilities (with rationale)

### 1. scimath module (Fitting, Functionals, Interpolation, Statistics, FFT)

**Rationale**: These are pure numerical algorithms with no persistence or
interoperability impact. Users can use Eigen, FFTW, or standard C++ libraries
for equivalent functionality. The rolling plan marks scimath as
KEEP-CAPABILITY with library-backed implementations, but the algorithms
themselves are not required for file-format or query interop.

**Impact**: None for table/MS/image interoperability. Users needing fitting or
statistics can use established numerical libraries directly.

### 2. scimath_f (Fortran routines)

**Rationale**: Legacy Fortran implementations of math routines. The plan
explicitly defers these until benchmarked C++ replacements are available.
No persistence impact.

**Impact**: None for interoperability.

### 3. fits module (FitsIO, FITSTable, FITSReader)

**Rationale**: The rolling plan specifies direct cfitsio usage as acceptable.
A custom C++ FITS class stack adds maintenance burden without interop benefit.

**Impact**: Users use cfitsio directly for FITS I/O. No table-format impact.

### 4. msfits module (MSFitsInput, MSFitsOutput, MSFitsIDI)

**Rationale**: These are conversion tools between FITS and MS formats. They
are not core library behavior and can be implemented as standalone utilities
if needed later.

**Impact**: No impact on MS table interop. Conversion tools are out of scope.

### 5. derivedmscal (DerivedMSCal UDFs)

**Rationale**: Virtual derived columns (hour angle, parallactic angle, LAST)
are convenience features that can be computed by users. They do not affect
stored data interoperability.

**Impact**: Low. Users can compute HA/PA/LAST from existing measures APIs.

### 6. MeasComet (comet ephemeris)

**Rationale**: Specialized ephemeris table access for solar system objects.
Not required for standard radio astronomy MS interop.

**Impact**: Very low. Comet tracking is a niche use case.

## Storage manager fidelity notes

See `docs/phase11/storage_manager_fidelity_audit.md` for detailed findings.
All reader/writer paths for the 6 required storage managers are classified as
`Exact` based on the Phase 7-10 interop matrix results (all cells pass).
