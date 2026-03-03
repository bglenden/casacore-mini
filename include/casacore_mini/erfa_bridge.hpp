// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <array>

namespace casacore_mini {

/// @file
/// @brief Thin C++ wrappers around ERFA astronomical functions.
/// @ingroup measures
/// @addtogroup measures
/// @{

///
/// This header provides a thin, exception-safe C++ wrapper layer over the
/// ERFA (Essential Routines for Fundamental Astronomy) C library.  Each
/// function maps closely to its ERFA counterpart, converting error codes to
/// `std::runtime_error` exceptions and using C++ value semantics where the
/// ERFA interface uses output pointers.
///
/// Time scale conversions (UTC, TAI, TT, UT1, TDB) use the standard
/// two-part Julian Date representation `(jd1, jd2)` where the split is
/// typically `jd1 = 2400000.5` (the Modified Julian Date epoch) and
/// `jd2 = MJD`.
///
/// Coordinate conversions use angular values in radians throughout.
///
/// Error handling:
/// - ERFA return values of 0 (OK) and +1 (dubious year/date, acceptable)
///   are treated as success.
/// - Return values <= -1 (unacceptable date, internal error) throw
///   `std::runtime_error`.
///

/// UTC to TAI. Input/output as 2-part JD (typically JD = 2400000.5 + MJD).
/// @throws std::runtime_error if ERFA returns an error status.
void utc_to_tai(double utc1, double utc2, double& tai1, double& tai2);

/// TAI to UTC.
void tai_to_utc(double tai1, double tai2, double& utc1, double& utc2);

/// TAI to TT (Terrestrial Time = TDT + 32.184s offset from TAI).
void tai_to_tt(double tai1, double tai2, double& tt1, double& tt2);

/// TT to TAI.
void tt_to_tai(double tt1, double tt2, double& tai1, double& tai2);

/// UTC to UT1 given dut1 = UT1-UTC in seconds.
void utc_to_ut1(double utc1, double utc2, double dut1, double& ut11, double& ut12);

/// UT1 to UTC given dut1 = UT1-UTC in seconds.
void ut1_to_utc(double ut11, double ut12, double dut1, double& utc1, double& utc2);

/// TT to TDB. dtr = TDB-TT in seconds (from erfa_dtdb or a simplified model).
void tt_to_tdb(double tt1, double tt2, double dtr, double& tdb1, double& tdb2);

/// TDB to TT.
void tdb_to_tt(double tdb1, double tdb2, double dtr, double& tt1, double& tt2);

/// Approximate TDB-TT in seconds for a given TT date.
/// Uses the ERFA eraDtdb model with simplified parameters (geocenter approximation).
double approx_dtdb(double tt1, double tt2);

/// Convert JD (two-part) to Besselian epoch (e.g. 1950.0).
double jd_to_besselian_epoch(double jd1, double jd2);

/// Geodetic (WGS84) to geocentric (ITRF) XYZ.
/// @param elong  Longitude in radians (east-positive).
/// @param phi    Latitude in radians (geodetic).
/// @param height Height above ellipsoid in meters.
/// @param xyz    Output ITRF Cartesian coordinates in meters.
void wgs84_to_itrf(double elong, double phi, double height, double (&xyz)[3]);

/// Geocentric (ITRF) XYZ to geodetic (WGS84).
void itrf_to_wgs84(const double (&xyz)[3], double& elong, double& phi, double& height);

/// ICRS/J2000 equatorial (ra, dec) to Galactic (l, b), all in radians.
void j2000_to_galactic(double ra, double dec, double& l, double& b);

/// Galactic (l, b) to ICRS/J2000 equatorial (ra, dec), all in radians.
void galactic_to_j2000(double l, double b, double& ra, double& dec);

/// FK4 B1950 to FK5 J2000 (position-only), using the given Besselian epoch.
void b1950_to_j2000(double ra_b1950, double dec_b1950, double bepoch, double& ra_j2000,
                    double& dec_j2000);

/// FK5 J2000 to FK4 B1950 (position-only), using the given Besselian epoch.
void j2000_to_b1950(double ra_j2000, double dec_j2000, double bepoch, double& ra_b1950,
                    double& dec_b1950);

/// Compute the IAU 2006/2000A bias-precession-nutation 3x3 rotation matrix.
/// Rotates J2000 → intermediate (CIP/CIO frame) for a given TT date.
void bias_precession_nutation_matrix(double tt1, double tt2,
                                     std::array<std::array<double, 3>, 3>& rbpn);

/// Apply a 3x3 rotation matrix to a direction (ra, dec) → (ra_out, dec_out).
/// Converts spherical→Cartesian, multiplies, converts back.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void rotate_direction(const std::array<std::array<double, 3>, 3>& mat, double ra_in, double dec_in,
                      double& ra_out, double& dec_out);

/// Apply the inverse of a 3x3 rotation matrix (transpose) to a direction.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void rotate_direction_inverse(const std::array<std::array<double, 3>, 3>& mat, double ra_in,
                              double dec_in, double& ra_out, double& dec_out);

/// Mean obliquity of the ecliptic for a given TT date (IAU 2006), in radians.
double mean_obliquity(double tt1, double tt2);

/// J2000 equatorial to ecliptic, using mean obliquity at J2000.0.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void j2000_to_ecliptic(double ra, double dec, double& elon, double& elat);

/// Ecliptic to J2000 equatorial.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ecliptic_to_j2000(double elon, double elat, double& ra, double& dec);

/// @}
} // namespace casacore_mini
