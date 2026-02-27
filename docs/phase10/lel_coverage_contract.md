# Phase 10 LEL Coverage Contract

## Required expression categories

Each category must include:
1. Parse success/failure parity checks
2. Deterministic evaluation parity vs casacore on fixed fixtures
3. Malformed-expression negative tests

---

## Category 1: Scalar/lattice arithmetic

### Required operators
- `+`, `-`, `*`, `/` between two lattices
- `+`, `-`, `*`, `/` between lattice and scalar (both orderings)
- Unary `-` and `+`

### Required behavior
- Element-wise operation with shape broadcast where applicable
- Division by zero produces IEEE inf/nan (not exception)
- Mixed Float/Double promotes to Double

### Parity fixtures
- Two 8x8 Float lattices with known values
- Scalar operands: 0.0, 1.0, -2.5, 1e30

---

## Category 2: Comparison and logical operators

### Required operators
- `==`, `!=`, `>`, `>=`, `<`, `<=` (lattice vs lattice, lattice vs scalar)
- `&&`, `||`, `!` (logical AND, OR, NOT on Bool lattices)

### Required behavior
- Comparison produces Bool lattice
- Logical operators require Bool operands
- Type mismatch is a parse error

### Parity fixtures
- Float lattice compared against thresholds
- Bool lattice from comparison used in logical expression

---

## Category 3: Mask-aware boolean evaluation

### Required behavior
- Masked pixels excluded from logical evaluation
- `iif(condition, true_val, false_val)` conditional expression
- `value(expr)` extracts unmasked values
- `mask(expr)` extracts mask as Bool lattice

### Parity fixtures
- Lattice with known mask, evaluated through conditional

---

## Category 4: Reductions

### Required functions
- `min()`, `max()` - global minimum/maximum
- `sum()` - sum of all unmasked elements
- `mean()` - mean of unmasked elements
- `median()` - median of unmasked elements
- `variance()`, `stddev()` - variance and standard deviation
- `ntrue()`, `nfalse()` - count of true/false in Bool lattice
- `any()`, `all()` - existential/universal Bool reductions

### Required behavior
- Reductions produce scalar result
- Mask-aware: only unmasked elements contribute
- Empty (all-masked) reduction behavior matches casacore

### Parity fixtures
- 16x16 Float lattice with known values and partial mask

---

## Category 5: Mathematical functions

### Required functions
- `sin()`, `cos()`, `tan()`
- `asin()`, `acos()`, `atan()`, `atan2(lat1, lat2)`
- `exp()`, `log()`, `log10()`, `sqrt()`
- `abs()`, `sign()`, `round()`, `ceil()`, `floor()`
- `pow(base, exp)`, `fmod(a, b)`
- `min(a, b)`, `max(a, b)` (element-wise two-argument forms)

### Required behavior
- Element-wise application
- Domain errors (sqrt of negative, log of zero) produce NaN/inf
- Mask propagation through function application

### Parity fixtures
- 8x8 Float lattice with values spanning function domains

---

## Category 6: Scalar-lattice broadcasting

### Required behavior
- Scalar promoted to constant lattice matching partner shape
- Scalar on left or right of binary operator
- Nested expressions with mixed scalar/lattice operands
- Shape mismatch between two lattices is a parse/eval error

### Parity fixtures
- Expressions: `2.0 * lat + 1.0`, `lat1 + lat2` (same shape),
  `lat1 + lat3` (different shape, expect error)

---

## Category 7: Complex number operations

### Required functions
- `real()`, `imag()`, `conj()`, `abs()`, `arg()`
- `formComplex(re, im)` - construct complex from real/imag lattices
- Arithmetic on Complex lattices

### Required behavior
- `real()`/`imag()` extract Float from Complex
- `formComplex()` constructs Complex from two Float lattices
- Mixed Complex/Float arithmetic promotes Float to Complex

### Parity fixtures
- 8x8 Complex lattice with known values

---

## Category 8: Malformed-expression diagnostics

### Required negative tests
- Syntax errors: unmatched parentheses, missing operand, unknown function
- Type errors: arithmetic on Bool, logical on Float
- Shape errors: binary op on incompatible shapes
- Undefined lattice reference in expression string

### Required behavior
- Parse failure produces descriptive error (not crash/segfault)
- Error message identifies location or nature of problem
- Partial evaluation does not occur on malformed expressions
