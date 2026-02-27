# P10-W6 Design Decisions

## Template AST over runtime variant dispatch
Each AST node is `LelNode<T>` with the element type baked in at compile time.
This avoids runtime type switches in the hot evaluation path while keeping
the API type-safe. The `LatticeExprNode` wrapper uses a variant of
`shared_ptr<LelNode<T>>` for type-erased storage when the concrete type
is not known until runtime.

## Bool specialization strategy
`LelBinary<bool>::eval()` and `LelUnary<bool>::eval()` are explicitly
specialized because arithmetic operations (+, -, *, /) don't apply to bool.
Instead, bool binary uses logical AND/OR and bool unary uses logical NOT.
The generic template handles float/double arithmetic. Both are explicitly
instantiated to avoid linker issues.

## Mask propagation
All binary/compound nodes AND-combine their children's masks. A masked (false)
element in either child produces a masked result. Reductions skip masked
elements. This matches casacore's LEL semantics.

## Scalar broadcasting
Scalars are broadcast to the partner lattice's shape during eval. This is
done lazily at eval time rather than at construction, keeping the AST compact.

## LelResult<T> as value+mask pair
Evaluation returns `LelResult<T>` containing a `vector<T>` of values and an
`optional<vector<bool>>` mask. The optional mask avoids allocation when no
masking is needed (the common case).
