# P11-W4 Design Decisions

## D1: Integer vs float arithmetic
Integer arithmetic (both operands int64_t) stays in int64_t. Mixed or float operands promote to double. Power (`**`) on integers uses std::pow then casts back to int64_t.

## D2: UPDATE flush semantics
The evaluator calls `table.flush()` after any UPDATE that modifies rows. Without flush, writes may not be visible to subsequent reads due to storage manager buffering.

## D3: DELETE semantics
DELETE collects matching row indices in `result.rows` and sets `result.affected_rows` but does not physically remove rows. The Table API has no `removeRow()` method — callers use the row set for downstream filtering.

## D4: Type coercion in comparisons
`compare_values()` compares: strings via `<`/`==`, numbers via double promotion, mixed string/number treats strings as less. This follows TaQL's weak typing model.

## D5: Short-circuit evaluation
AND and OR use short-circuit evaluation: `false AND x` returns false without evaluating x; `true OR x` returns true without evaluating x.

## D6: String function placement
String functions (STRLEN, UPCASE, DOWNCASE, TRIM) are checked before `as_double(args[0])` in the 1-arg function handler to avoid type conversion errors on string arguments.

## D7: clang-tidy compliance
Used `TaqlValue{expr}` explicit construction to avoid `bugprone-narrowing-conversions` false positives. Used `static_cast<bool>` and explicit `bool` variables for `readability-implicit-bool-conversion`. Used `[[fallthrough]]` and `NOLINT` for intentional `bugprone-branch-clone` cases.
