// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief FITS keyword <-> CoordinateSystem conversion.

/// Build a CoordinateSystem from FITS-style header keywords stored in a Record.
///
///
/// Expects WCS keywords stored as `Record` fields:
/// `CTYPE<N>`, `CRVAL<N>`, `CRPIX<N>`, `CDELT<N>`, `NAXIS<N>`, and
/// optionally `PC<i>_<j>` or `CD<i>_<j>` cross-terms.
///
/// Axis type recognition rules:
/// - `"RA---XXX"` / `"DEC--XXX"` — `DirectionCoordinate` (equatorial J2000).
/// - `"GLON-XXX"` / `"GLAT-XXX"` — `DirectionCoordinate` (Galactic).
/// - `"FREQ"` / `"VELO"` — `SpectralCoordinate`.
/// - `"STOKES"` — `StokesCoordinate`.
/// - Any other `CTYPE` value — `LinearCoordinate`.
///
/// `coordinate_system_to_fits_header` is the inverse: it serializes a
/// `CoordinateSystem` back to a `Record` of WCS FITS keywords suitable for
/// writing to a FITS header or an image keyword table.
///
///
/// @par Example
/// @code{.cpp}
///   Record hdr;
///   hdr.fields["NAXIS1"] = RecordValue(std::int64_t{512});
///   hdr.fields["CTYPE1"] = RecordValue(std::string{"RA---SIN"});
///   hdr.fields["CRVAL1"] = RecordValue(83.8221);  // degrees
///   hdr.fields["CRPIX1"] = RecordValue(256.0);
///   hdr.fields["CDELT1"] = RecordValue(-4.84813e-6);  // radians/pixel
///   // ... add remaining axes ...
///   auto cs = coordinate_system_from_fits_header(hdr);
/// @endcode
///
///
/// @throws std::invalid_argument on unrecoverable keyword inconsistencies.
[[nodiscard]] CoordinateSystem coordinate_system_from_fits_header(const Record& fits_keywords);

/// Convert a CoordinateSystem to FITS-style header keywords.
///
/// Writes CTYPE, CRVAL, CRPIX, CDELT, PC, and related keywords for each axis.
[[nodiscard]] Record coordinate_system_to_fits_header(const CoordinateSystem& cs);

} // namespace casacore_mini
