# P11-W6 Open Issues

## O1: Correlation WHERE clause
The corr expression maps to correlation/polarization types (XX, YY, RR, LL). Generating a WHERE clause for this requires joining against the POLARIZATION subtable to resolve correlation names to DATA_DESC_ID values. Currently not implemented in `to_taql_where()` — would need subtable lookup.

## O2: Feed pair WHERE clause
Feed pair expressions (e.g., `0&1`) generate complex WHERE conditions involving both FEED1 and FEED2 columns with AND/OR logic. The current implementation handles simple feed ID lists but does not generate the full pair-cross WHERE syntax. The direct `evaluate()` path handles this correctly.

## O3: UVW distance WHERE clause
UV-distance filtering requires `SQRT(SUMSQR(UVW[0],UVW[1]))` which uses array element access. This depends on TaQL array indexing support which is not yet implemented.

## O4: SPW channel range WHERE clause
SPW expressions with channel ranges (e.g., `0:0~63`) filter at the channel level, not the row level. A WHERE clause can only filter DATA_DESC_ID, not individual channels. Channel selection is better handled at the data access layer rather than via WHERE.
