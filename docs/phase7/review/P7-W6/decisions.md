# P7-W6 Decisions

## Autonomous decisions made

1. Used a pragmatic table directory approach: structural metadata + SM file
   pass-through rather than full bucket-level SSM read/write. Cell-level
   verification delegated to casacore interop tool.
2. Fixed TableDesc v2 userType field parsing (was reading as u32 instead of
   string). This was a bug discovered during interop testing, not part of the
   original W6 scope.

## Assumptions adopted

1. SM data files are opaque blobs in the directory layer; interpretation is
   deferred to storage-manager-specific readers.

## Tradeoffs accepted

1. mini->casacore directory verification is expected to fail at W6 since
   mini does not produce SM data files. Accepted as a known limitation.
