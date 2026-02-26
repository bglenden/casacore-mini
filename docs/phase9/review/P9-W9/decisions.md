# P9-W9 Decisions

1. **Lazy caching in MsMetaData**: Each category (antenna, field, spw, obs,
   main-table) is loaded on first access and cached. Subsequent calls return
   references to cached vectors/sets without re-reading disk.

2. **MsSummary as free function**: `ms_summary()` produces a string rather
   than writing to a stream, keeping the API simple and testable.

3. **Frequencies displayed in MHz**: SPW reference frequencies are divided
   by 1e6 for human-readable display in the summary output.

4. **MsMetaData delegates to MsXxxColumns**: Reuses the typed column wrapper
   classes from W4 rather than directly reading SSM, maintaining abstraction
   layering.
