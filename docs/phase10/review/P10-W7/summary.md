# P10-W7 Review: LEL Parser and Operator/Function Coverage

## Scope
Implement the LEL string parser, complete operator/function coverage for all
8 contracted categories, and add malformed-expression diagnostics.

## Deliverables
| Artifact | Lines | Status |
|----------|-------|--------|
| `include/casacore_mini/lattice_expr.hpp` | +250 | Extended |
| `src/lattice_expr.cpp` | +500 | Extended |
| `tests/lel_parser_test.cpp` | ~730 | New |
| `CMakeLists.txt` | +3 lines | Modified |
| `tools/check_phase10_w7.sh` | ~40 | Updated from stub |

## New Capabilities

### LEL String Parser
- Recursive descent parser with operator precedence
- Supports: arithmetic (+,-,*,/), comparison (==,!=,>,>=,<,<=),
  logical (&&,||,!), parentheses, function calls, iif()
- `LelSymbolTable` maps lattice names to typed nodes
- `LelParseError` with position reporting

### value() / mask() Extractors (Category 3)
- `LelValueExtract<T>`: strips mask, returns just values
- `LelMaskExtract<T>`: extracts mask as Bool lattice (true=unmasked)

### Complex Number Operations (Category 7)
- `LelReal<R>`, `LelImag<R>`: extract real/imaginary parts
- `LelConj<R>`: conjugate
- `LelArg<R>`: phase angle
- `LelComplexAbs<R>`: magnitude (|z|)
- `LelFormComplex<R>`: construct complex from two real lattices
- Complex arithmetic via `LelBinary<complex<R>>`

### Float→Double Promotion
- `LelPromoteFloat`: promotes float nodes to double
- Parser auto-promotes when mixing float and double operands

### Bool Reduction Factory
- `lel_bool_reduce<R>()` factory for ntrue, nfalse, any, all

## LEL Coverage Contract Status
| Category | Status | Test Count |
|----------|--------|------------|
| 1. Scalar/lattice arithmetic | Complete | 20+ |
| 2. Comparison and logical | Complete | 18+ |
| 3. Mask-aware boolean eval | Complete | 12+ |
| 4. Reductions | Complete | 18+ |
| 5. Mathematical functions | Complete | 30+ |
| 6. Scalar-lattice broadcasting | Complete | 8+ |
| 7. Complex number operations | Complete | 15+ |
| 8. Malformed-expression diagnostics | Complete | 17+ |

## Test Results
- **lel_parser_test**: 169 assertions passed
- **lattice_expr_test**: 65 assertions passed (unchanged from W6)
- **Full regression**: 74/74 tests passed (73 prior + 1 new)
