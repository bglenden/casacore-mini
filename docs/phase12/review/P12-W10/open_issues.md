# P12-W10 Open Issues

1. Lock model is advisory, not enforced. Read/write operations do not check
   the lock state — callers must explicitly acquire and check locks. This
   matches casacore's behavior where locking is opt-in.
2. No multi-process lock contention handling. The lock file approach works
   for single-process use and simple coordination, but does not provide
   atomic lock acquisition across processes.
3. `set_table_info()` writes to disk immediately but `table_info()` reads
   from disk each time. No caching is done since info changes rarely.
4. TaQL ALTER TABLE does not yet acquire locks before mutation. This would
   be a natural follow-up in W11.
