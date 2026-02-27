# P11-W12 Decisions

1. **Checklist zero-Pending enforcement**: The W12 gate script verifies
   that both checklists contain zero rows with "Pending" status. All
   capabilities are either Done, Simplified, or Excluded.

2. **Cascading gate verification**: W12 runs all W1-W11 gates to ensure
   no regressions were introduced during later waves.

3. **Review packet completeness**: Every wave has a review directory with
   summary, decisions, and open_issues documents.
