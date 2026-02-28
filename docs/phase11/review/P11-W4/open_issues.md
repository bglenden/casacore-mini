# P11-W4 Open Issues

## O1: INSERT command stub
INSERT command parsing is complete (W3) but execution returns an empty result with a TODO. Implementing INSERT requires adding rows to a table, which may need Table API extensions. Low priority for closure phase.

## O2: CREATE/ALTER/DROP TABLE stubs
Table DDL commands parse but execution is stubbed. These require Table factory methods that are outside current scope.

## O3: ROWNR/ROWID functions
Currently return 0 as placeholders. They need access to the evaluation context's current row index, which is available but not yet wired through.

## O4: Complex number evaluation
Complex literals parse correctly but most math functions don't handle complex operands. Functions like ABS, REAL, IMAG, NORM, ARG are present but only handle double inputs. Complex-aware evaluation would need std::complex specializations.

## O5: Array/vector operations
Set expressions build `vector<double>` but no array reduction functions (gmin, gmax, gmean, etc.) are implemented. These are needed for full aggregate support.

## O6: GIVING clause
The GIVING clause (output table) is parsed but not wired into command execution. Would need table creation + write support.
