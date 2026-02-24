# P7-W8 Open Issues

## mini→casacore EXPECTED_FAIL for all directory cases
Mini produces table.dat metadata but no SM data files (table.f0, etc.), so
casacore cannot open mini-produced tables. This is the expected behavior at the
directory-level structural interop scope. Full bucket-level SM implementations
are deferred per `docs/phase7/deferred_managers.md`.

## OMP /tmp warnings during matrix run
Casacore's OpenMP runtime emits "Can't set size of /tmp file" warnings in
sandboxed environments. These are benign and do not affect matrix results.
