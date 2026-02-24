# CLAUDE.md

This file is only for Claude Code-specific guidance.

Project scope, phase definitions, quality gates, and required review artifacts
are defined in:

1. `docs/casacore_plan.md` (rolling source of truth)
2. active phase plan (currently `docs/phase7/plan.md`)

## Claude Code-specific notes

1. Use non-interactive commands only (no interactive rebases, patch add, or
   editor-driven flows).
2. When command output is too long/truncated, rerun with narrower scope and
   save outputs to files under `docs/phaseN/review/` when evidence is needed.
3. If a git operation is interrupted and `.git/index.lock` remains, confirm no
   active git process before removing the lock file.
4. The original casacore headers are available in `../casacore-original/` for
   reference when working on the casacore interop tool.
