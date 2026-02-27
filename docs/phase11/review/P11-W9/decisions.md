# P11-W9 Decisions

1. **mini->mini only**: Cross-toolchain cells require casacore-original
   toolchain which is not available in this build environment. These are
   documented in known_differences.md rather than tested.

2. **Existing test suites as matrix evidence**: Rather than creating
   separate matrix test binaries, the gate script runs the existing test
   suites and maps their pass/fail to matrix cells. This avoids code
   duplication while providing the same coverage evidence.
