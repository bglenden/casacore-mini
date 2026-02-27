# P10-W4 Decisions

## D1: CoordinateSystem deep copy via save/restore

**Decision:** SubImage uses `CoordinateSystem::restore(parent.coordinates().save())`
rather than copy construction.

**Rationale:** CoordinateSystem contains `unique_ptr<Coordinate>` members in its
CoordEntry vector, making it implicitly non-copyable. The save/restore roundtrip
through Record provides a correct deep copy that clones all coordinate objects.

## D2: TempImage takes CoordinateSystem by move

**Decision:** `TempImage(IPosition, CoordinateSystem)` takes the coordinate system
by value, expecting callers to move or construct inline.

**Rationale:** Avoids copying non-copyable CoordinateSystem. Consistent with
image construction semantics where the image owns its coordinate system.

## D3: PagedImage metadata in table keywords

**Decision:** PagedImage persists metadata as table keywords:
- `coords` → CoordinateSystem Record (via save/restore)
- `imageinfo` → ImageInfo Record (via to_record/from_record)
- `miscinfo` → user Record
- `units` → string

**Rationale:** Matches casacore's PagedImage table keyword layout. Keywords
are flushed alongside pixel data.

## D4: PagedImage persistence test deferral

**Decision:** Full create→flush→reopen→read roundtrip tests deferred to W8.

**Rationale:** Keyword persistence through flush requires the table writer to
serialize Record keywords to table.dat, which is a mutation-integrity concern
best tested in W8 alongside other persistence roundtrip scenarios.
