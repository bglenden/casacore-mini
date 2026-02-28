# P11-W10 Decisions

1. **Reuse test suite runs**: The full-project matrix reuses the same test
   binaries already exercised in W9, verifying the complete set in one pass
   rather than introducing separate matrix test infrastructure.

2. **Prior-phase script existence check**: W10 verifies that interop matrix
   scripts from phases 7, 9, and 10 exist. Running them is left to CI
   since they may have additional dependencies.
