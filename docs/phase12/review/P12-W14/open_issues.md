# P12-W14 Open Issues

1. **Interop matrix runs require casacore-enabled host**: Phase-8, Phase-10,
   and Phase-11 matrix suites cannot run on this host (no casacore installed).
   Wave gates validate build, test, and functional claims locally. Full
   interop matrix runs are deferred to a casacore-enabled environment.

2. **Coverage report not generated**: Coverage builds (`check_ci_build_lint_test_coverage.sh`)
   are not part of W14 closeout scope. Coverage can be run separately when needed.
