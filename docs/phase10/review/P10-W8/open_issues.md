# P10-W8 Open Issues

## O1: Mixed SSM+TSM flush
Table::flush uses an else-if chain — only one storage manager writer gets
flushed per call. If a table has both SSM and TSM columns, only the first
matching writer flushes. Not a problem today (PagedImage pixels are TSM-only,
metadata is in table keywords), but could be an issue for future mixed tables.

## O2: No partial-write / corruption guard tests
The W8 plan mentioned "corruption/partial-write guard behavior tests where
supported." We don't currently have a mechanism to simulate partial writes
or verify recovery. This is deferred; the infrastructure doesn't support it
without additional work.
