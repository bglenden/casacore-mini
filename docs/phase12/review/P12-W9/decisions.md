# P12-W9 Decisions

1. ParseLate is functionally equivalent to ParseNow: expressions are always
   validated at `evaluate()` time (not at `set_*_expr()` time). The enum
   exists for API compatibility with upstream casacore's `MSSelection`.
2. Error-collection wraps each category independently, so a failure in one
   category (e.g., antenna) does not prevent other categories (e.g., scan)
   from being evaluated. The `errors` vector accumulates all exceptions.
3. For ID-list antenna expressions (no `&` operator), `antenna1_list` and
   `antenna2_list` are set to the same values as `antennas` — this mirrors
   upstream behavior where a simple ID list means "any antenna on either side."
4. `feed1_list` / `feed2_list` follow the same pattern as antenna lists:
   populated from pair expressions, mirrored from simple ID lists.
5. `ddid_to_pol_id` maps DDID → POLARIZATION_ID (row index in the
   POLARIZATION subtable), not to the actual correlation type list. This
   matches upstream casacore's convention.
6. `to_taql_where()` state pattern resolution changed from `glob_match` to
   `name_match` to be consistent with the evaluator (which uses regex+glob).
