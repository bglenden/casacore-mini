# P10-W4 Open Issues

- PagedImage create→flush→reopen→read roundtrip not yet tested; deferred to W8
  (mutation integrity wave) since it requires keyword serialization through
  the table writer flush path.
- CoordinateSystem lacks a copy constructor. If needed beyond SubImage, a
  `clone()` method wrapping save/restore could be added to the class directly.
- image_test uses -DCASACORE_MINI_WARNINGS_AS_ERRORS=OFF due to aggregate
  initialization warnings from Projection{type, {}}. Could add a Projection
  constructor to avoid this.
