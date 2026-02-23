# Phase 3 keyword fixture provenance

Date: 2026-02-23

These replay fixtures were captured from local casacore build artifacts rooted
at:

- `${CASACORE_BUILD_ROOT}` = `/Users/brianglendenning/SoftwareProjects/casacore/main/build`

Capture command pattern:

```bash
${CASACORE_BUILD_ROOT}/tables/apps/showtableinfo \
  in=<artifact_path> dm=t col=t tabkey=t colkey=t maxval=25
```

After capture, absolute `${CASACORE_BUILD_ROOT}` paths were rewritten to the
literal placeholder `${CASACORE_BUILD_ROOT}` for deterministic, host-independent
fixtures.

Captured fixtures:

1. `logtable_stdstman_keywords/showtableinfo.txt`
2. `ms_tree/showtableinfo.txt`
3. `pagedimage_coords/showtableinfo.txt`
