# P11-W1 Decisions

## Key disposition decisions

1. **Include TaQL full surface** (C11-T1): Non-deferable per rolling plan.
   10 command families, 220+ functions, full expression grammar.

2. **Include MSSelection full surface** (C11-T2): Non-deferable per rolling plan.
   12 category parsers, result accessors, error handling.

3. **Include toTableExprNode bridge** (C11-T3): Required for active query paths.

4. **Include MeasUDF**: TaQL measure UDFs are commonly used in radio
   astronomy workflows. Implemented through UDF extension point.

5. **Include SM writer integration**: Table::create() must support all 6
   required storage managers.

6. **Exclude scimath/scimath_f/fits/msfits/derivedmscal**: No persistence
   or interop impact. Users can use established libraries directly.

## Tranche mapping confirmed

- P11-W3: TaQL parser + AST
- P11-W4: TaQL evaluator + execution + SM integration + MeasUDF
- P11-W5: MSSelection parsers
- P11-W6: MSSelection evaluator + bridge
- P11-W7: Closure and integration
