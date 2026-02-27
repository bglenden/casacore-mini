# P10-W7 Design Decisions

## Recursive descent parser
Chose a hand-written recursive descent parser over a grammar generator.
This keeps the dependency footprint at zero and provides clear error
messages with position tracking. The grammar is small enough (~6 precedence
levels) to be maintainable.

## LelParseError with position
Parse errors include the byte position in the input string, matching
casacore's LEL error behavior. This enables callers to highlight the
error location in user-facing messages.

## LelSymbolTable for lattice references
Parser resolves identifiers through a symbol table rather than embedding
references directly in the expression string. This separates parsing from
lattice lifecycle management and makes the parser stateless.

## Auto float→double promotion
When the parser encounters mixed float/double operands (e.g., a float
lattice and a double scalar), it automatically inserts a LelPromoteFloat
node. This matches casacore's widening promotion behavior.

## Parser produces float by default for numeric literals
All numeric literals (e.g., "2.0", "1e3") are parsed as float by default.
This matches casacore's LEL behavior where expressions default to float.
Double precision is achieved through lattice type or explicit promotion.

## Separate complex node classes
Complex operations (real, imag, conj, arg, abs) use dedicated node classes
rather than overloading the generic LelFunc1. This avoids complex template
dispatch issues since the input type (complex<R>) differs from the output
type (R) for most operations.
