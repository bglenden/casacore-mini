# P9-W6 Decisions

1. **Free-function iterators over class-based**: `ms_iter_chunks()` and
   `ms_time_chunks()` are free functions returning vectors rather than
   stateful iterator objects, matching the project's preference for simple
   value-oriented APIs.

2. **StokesConverter uses standard radio interferometry formulas**: Circular
   (RR/RL/LR/LL) and linear (XX/XY/YX/YY) to Stokes (I/Q/U/V) conversions
   use the canonical formulas. Passthrough mode returns input unchanged when
   correlations already match output type.

3. **MsDopplerUtil delegates to VelocityMachine**: Wraps existing Phase 8
   `radio_velocity_to_freq()` and `freq_ratio_to_radio_velocity()` rather
   than re-implementing Doppler math.

4. **MsHistoryHandler uses Unix→MJD conversion**: Converts `system_clock::now()`
   to MJD seconds via the MJD epoch offset (40587 days).

5. **Dropped MsTileLayout / MSRange**: Plan mentioned these but they add no
   value beyond what existing SM writers and iterator functions already provide.
