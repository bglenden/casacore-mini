# P11-W11 Decisions

1. **22 known differences documented**: Each difference maps to a specific
   upstream feature and explains the current casacore-mini status (parser
   complete, evaluator limited, or not implemented) with rationale.

2. **Malformed input coverage**: The `ms_selection_malformed_test` exercises
   invalid syntax, out-of-range indices, empty strings, and type mismatches
   to verify graceful error handling.

3. **Doxygen-style comments**: Used `///` style for public API headers to
   enable documentation generation without adding a Doxygen build step.
