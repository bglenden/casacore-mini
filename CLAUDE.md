# CLAUDE.md

- Rolling plan: `docs/casacore_plan.md`. Active phase plan: `docs/phase7/plan.md`.
- Original casacore headers live in `../casacore-original/` (reference only).
- Non-interactive commands only — no interactive rebase, `git add -p`, etc.
- Long/truncated output: rerun narrower and save to `docs/phaseN/review/`.
- After dataset creation, do not use storage managers to read data directly, instead use the public read/write methods of the high-level data structure (table, image, measurement set, etc.)
- Use mdspan for multidimensional arrays rather than our own array classes
