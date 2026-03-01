# P12-W2 Decisions

1. **Flush-rebuild strategy**: add_row and remove_row flush to disk, then
   rebuild writers with the new row count. This is correct and maintainable
   since writers are designed around fixed row counts at setup time.

2. **Default values for new rows**: New rows are zero-initialized by the
   writer's bucket/tile allocation. Callers can then write specific values.

3. **No batch remove_row**: Only single-row removal is implemented initially.
   A batch variant can be added later if needed.
