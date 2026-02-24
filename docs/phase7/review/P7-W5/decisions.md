# P7-W5 Decisions

## D1: Complex numbers demote to string

Complex scalar and array values have no representation in the text-model
`KeywordRecord`. They are serialized as `"(real,imag)"` strings rather than
throwing, because the text path is inherently lossy and consumers should use
the binary path when precision matters.

## D2: Integer/float widening on KV→RV

`KeywordValue` stores all integers as `int64_t` and all floats as `double`.
The conversion to `RecordValue` preserves these wide types rather than
attempting to narrow (e.g., to `int32_t` or `float`), because the text parser
has already lost the original type information.

## D3: Array shape lost on RV→KV

Multi-dimensional `RecordArray` values are flattened to 1-D `KeywordArray`
because the text model has no shape representation. This is an inherent
limitation of the text path; the binary path is authoritative for shape.

## D4: RecordList → KeywordArray

`RecordList` (heterogeneous ordered collection) maps to a flat `KeywordArray`
with each element individually converted. The type heterogeneity is preserved
at the element level (mixed int64/string/etc. elements).

## D5: Text path is inherently lossy

Documented in the header: the text parser loses type precision (all ints →
int64, all floats → double, no complex, no shape). A KV→RV→KV roundtrip
preserves values within the text type system, but an RV→KV→RV roundtrip
widens narrow types. Binary path is authoritative.
