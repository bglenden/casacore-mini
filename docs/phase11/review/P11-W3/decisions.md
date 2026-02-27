# P11-W3 Decisions

1. **Hand-written parser over generated**: Chose recursive descent over bison/flex for cleaner C++23 code, easier debugging, and single-file implementation.

2. **Single-token lookahead with lexer snapshot**: For `&&`/`||` disambiguation, the parser snapshots and restores the lexer state rather than maintaining a full lookahead buffer.

3. **Precedence climbing**: Expression parsing uses standard precedence climbing rather than Pratt parsing, matching upstream casacore's operator precedence.

4. **Evaluator stubs in W3**: `taql_execute()` and `taql_calc()` return empty results; full evaluation deferred to W4 as planned.

5. **NOLINT annotations**: Two clang-tidy `bugprone-branch-clone` suppressions added for legitimate cases (switch dispatch, complex literal variant).
