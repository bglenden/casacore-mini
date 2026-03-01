# P12-W9 Open Issues

1. ParseLate does not truly defer parsing — both modes validate at
   `evaluate()` time. A strict implementation would validate at
   `set_*_expr()` time for ParseNow and defer for ParseLate. This is
   acceptable because upstream casacore's MSSelection also validates at
   `toTableExprNode()` time regardless of mode.
2. `to_taql_where()` does not wrap individual categories in try-catch for
   error collection. It always throws on malformed expressions regardless
   of `error_mode_`.
3. The `UvUnit` stored in `uv_ranges` is always `Meters` even when the
   input used km (the value is pre-multiplied). Only wavelength/percentage
   modes would need a distinct unit, and those are not fully supported.
