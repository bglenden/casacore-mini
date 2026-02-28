# P11-W5 Open Issues

## O1: Antenna regex support
Upstream MSAntennaGram supports regex matching on antenna names (e.g., `/^DV[0-9]+$/`). Current implementation supports glob patterns (`DV*`) but not regex. Low priority since glob covers most use cases.

## O2: Field regex and bounds
Upstream MSFieldGram supports regex and `<N` / `>N` bounds for field IDs. Current implementation supports glob and name matching. Bounds would be straightforward to add.

## O3: SPW frequency-based selection
Upstream MSSpwGram supports frequency ranges with units (e.g., `1.0GHz~2.0GHz`). Current implementation only supports SPW by ID and channel ranges. Frequency selection would require reading channel frequencies from the SPW subtable.

## O4: Time format parsing
Upstream MSTimeGram supports human-readable date/time formats (`YYYY/MM/DD/HH:MM:SS.sss`). Current implementation only supports raw MJD values and simple `> < ~` operators.

## O5: UV-distance units
Upstream MSUvDistGram supports unit specification (m, km, wavelength). Current implementation assumes meters.

## O6: Station@location antenna syntax
Upstream supports `ant@station` syntax for location-based antenna selection. Not implemented.

## O7: Accessor methods
Selected-ID accessor methods (getAntenna1List, getFieldList, etc.) are deferred to W6 per the capability checklist.

## O8: Error handler infrastructure
Upstream MSSelection has configurable error handlers for collecting parse errors. Current implementation throws std::runtime_error immediately.
