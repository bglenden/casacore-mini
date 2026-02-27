# P9-W13 Decisions

1. Oracle verification remains strict: any mismatch or verifier crash is a gate failure.
2. Oracle verifier access policy remains `Table` API only; no direct storage-manager cell reads.
3. Reader compatibility fixes are treated as Phase 9 blocking correctness defects, not deferred cleanup.
