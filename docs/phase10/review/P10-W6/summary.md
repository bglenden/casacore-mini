# P10-W6 Review: Expression Engine Foundation

## Scope
Implement the core Lattice Expression Language (LEL) evaluation engine:
AST node hierarchy, evaluation with mask propagation, builder helpers,
and the `LatticeExpr<T>` / `LatticeExprNode` wrappers.

## Deliverables
| Artifact | Lines | Status |
|----------|-------|--------|
| `include/casacore_mini/lattice_expr.hpp` | ~320 | New |
| `src/lattice_expr.cpp` | ~580 | New |
| `tests/lattice_expr_test.cpp` | ~330 | New |
| `CMakeLists.txt` | +3 lines | Modified |
| `tools/check_phase10_w6.sh` | ~45 | Updated from stub |

## AST Node Hierarchy
- `LelNode<T>` — abstract base (shape, is_scalar, eval)
- `LelScalar<T>` — scalar constant
- `LelArrayRef<T>` — lattice reference leaf with optional mask
- `LelBinary<T>` — arithmetic binary ops (+, -, *, /)
- `LelCompare<T>` — comparison ops producing bool
- `LelUnary<T>` — unary ops (negate, identity, logical_not)
- `LelFunc1<T>`, `LelFunc2<T>` — 1-arg and 2-arg math functions
- `LelReduce<T>` — numeric reductions (sum, min, max, mean, median, variance, stddev)
- `LelBoolReduce<R>` — bool reductions (ntrue, nfalse, any, all)
- `LelIif<T>` — conditional expression (where cond ? true_expr : false_expr)

## Type-Erased Wrappers
- `LatticeExprNode` — variant<shared_ptr<LelNode<T>>> for float/double/bool
- `LatticeExpr<T>` — typed wrapper with `eval()` method

## Test Results
- **lattice_expr_test**: 65 assertions passed
- **Full regression**: 73/73 tests passed (72 prior + 1 new)

## Key Design Decisions
- Template-based AST rather than runtime type dispatch
- Bool specializations for `LelBinary<bool>` (AND/OR) and `LelUnary<bool>` (NOT)
- Mask propagation via AND-combine of child masks
- Scalar-lattice broadcasting handled transparently
- Explicit template instantiations for float, double, bool
