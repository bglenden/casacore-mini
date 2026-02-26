# P9-W4 Open Issues

## Blocking issues

None.

## Non-blocking issues

1. ANTENNA subtable array columns (POSITION, OFFSET) return empty vectors
   because SsmReader does not support array cell reads. Full array support
   for SSM-stored arrays is deferred to W5/W6 or a dedicated SSM array
   enhancement.

## Proposed follow-ups

1. SSM array cell reading capability (needed for subtable array columns).
