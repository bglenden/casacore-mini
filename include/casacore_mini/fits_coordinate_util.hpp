#pragma once

#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief FITS keyword <-> CoordinateSystem conversion.

/// Build a CoordinateSystem from FITS-style header keywords stored in a Record.
///
/// Expects WCS keywords: CTYPE1/2/3/..., CRVAL1/2/..., CRPIX1/2/...,
/// CDELT1/2/..., NAXIS1/2/..., optional PC1_1 etc.
///
/// Recognizes:
/// - "RA---XXX"/"DEC--XXX" -> DirectionCoordinate
/// - "GLON-XXX"/"GLAT-XXX" -> DirectionCoordinate (Galactic)
/// - "FREQ"/"VELO" -> SpectralCoordinate
/// - "STOKES" -> StokesCoordinate
/// - Other -> LinearCoordinate
///
/// @throws std::invalid_argument on unrecoverable keyword inconsistencies.
[[nodiscard]] CoordinateSystem coordinate_system_from_fits_header(const Record& fits_keywords);

/// Convert a CoordinateSystem to FITS-style header keywords.
///
/// Writes CTYPE, CRVAL, CRPIX, CDELT, PC, and related keywords for each axis.
[[nodiscard]] Record coordinate_system_to_fits_header(const CoordinateSystem& cs);

} // namespace casacore_mini
