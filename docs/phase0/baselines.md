# Phase 0 Baselines (`P0-W1`)

Date: 2026-02-23

## Scope
This document pins the source baselines and records fork-vs-upstream deltas that matter for interoperability planning.

## Pinned baselines

- Upstream compatibility baseline:
  - Repo: `https://github.com/casacore/casacore`
  - Remote ref: `upstream/master`
  - Commit: `dede86795`
- Local fork baseline used for current inventory:
  - Repo: `https://github.com/bglenden/casacore`
  - Branch: `master`
  - Commit: `64051d91a`

Divergence at capture time:
- `upstream/master...HEAD`: `0` behind, `28` ahead

## Working tree note
The local `casacore` checkout was dirty at capture time (uncommitted changes under `tables/DataMan` and `tables/TaQL`, plus untracked `Testing/`).

Phase-0 policy:
- Treat `64051d91a` as the pinned fork baseline commit.
- Do **not** treat uncommitted working-tree changes as baseline requirements until committed and reviewed.

## Fork delta summary vs upstream

Top-level changed-file counts (`upstream/master...HEAD`):

| Area | Files changed |
|---|---:|
| `tables` | 99 |
| `casa` | 32 |
| `ms` | 10 |
| `scimath` | 8 |
| `lattices` | 8 |
| `msfits` | 5 |
| `measures` | 3 |
| `images` | 3 |
| `coordinates` | 3 |
| other (build/docs/ci/python/etc.) | 18 |

`tables` submodule breakdown:

| Submodule | Files changed |
|---|---:|
| `tables/Tables` | 40 |
| `tables/TaQL` | 30 |
| `tables/DataMan` | 22 |
| `tables/LogTables` | 3 |
| `tables/apps` | 2 |
| `tables/Dysco` | 1 |
| `tables/AlternateMans` | 1 |

## Delta themes

1. Modernization/test-infra wave commits:
- CI/security hardening, warning baseline publication, coverage baseline, and broad CTest migration.

2. API modernization wave commits:
- Large `Block<T>` -> `std::vector<T>` migration across `tables`, `casa`, `ms`, `msfits`, and related dependencies.

## Persistence-impact triage (initial)

| Area | Initial impact on persistence compatibility | Why |
|---|---|---|
| `tables/Tables` | High | Core table descriptors, iterators, table operations, and public interfaces changed. |
| `tables/DataMan` | High | Storage-manager implementation surfaces changed; these back on-disk table data. |
| `tables/TaQL` | Medium-High | Query layer changes are not core storage format, but can affect functional parity and persisted expression workflows. |
| `casa` (`IO`, `Containers/Record`, `Quanta`) | High | These are compatibility-critical substrate areas; even signature-only refactors require validation. |
| `ms` / `msfits` | Medium-High | Schema/API-path updates over table persistence surfaces; potential behavioral differences. |
| `coordinates` / `images` / `lattices` / `measures` | Medium | Smaller delta count, but touches domain semantics tied to persisted metadata. |
| build/docs/test-only changes | Low | No direct file-format impact; still relevant for reproducibility and gating. |

## Phase-0 implications for next workstreams

1. `P0-W2` compatibility contract must explicitly separate:
- format-level parity requirements (strict), and
- API-level refactors (allowed if format parity holds).

2. `P0-W3` corpus must prioritize coverage for:
- table/storage-manager behavior (`Tables`, `DataMan`),
- TaQL behavior snapshots for representative expressions,
- MS/MSFits read-path parity fixtures.

3. Oracle/comparator (`P0-W4/P0-W5`) must include checks that detect:
- schema/keyword drift,
- scalar/array payload drift,
- storage-manager-binding differences.

## Repro commands

```bash
git -C /Users/brianglendenning/SoftwareProjects/casacore/main fetch upstream
git -C /Users/brianglendenning/SoftwareProjects/casacore/main rev-list --left-right --count upstream/master...HEAD
git -C /Users/brianglendenning/SoftwareProjects/casacore/main diff --name-only upstream/master...HEAD
```
