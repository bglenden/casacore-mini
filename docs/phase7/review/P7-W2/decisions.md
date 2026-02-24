# P7-W2 Decisions

## Autonomous decisions made

- Canonical record dump format uses base64 encoding for all string values
  (`b64:` prefix) to avoid delimiter/escaping ambiguity.
- Canonical record dump format uses `std::hexfloat` for all floating-point
  values to ensure bit-exact reproducibility across platforms.
- Verification oracle compares canonical dump lines rather than raw bytes,
  allowing semantic equivalence checks even when serialization padding differs.

## Assumptions adopted

- Corpus fixture `.bin` files are authoritative ground truth for record content.
- Both tools must produce byte-identical AipsIO output for identical input
  records (validated indirectly via canonical dump comparison).

## Tradeoffs accepted

- The canonical dump format is verbose but deterministic. Compactness was
  sacrificed for debuggability and cross-platform reproducibility.
