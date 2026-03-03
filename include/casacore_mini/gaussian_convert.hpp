// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/direction_coordinate.hpp"

#include <tuple>

namespace casacore_mini {

/// @file
/// @brief Convert Gaussian source shape parameters between direction coordinate frames.
///
///
///
///
/// Gaussian source fitting in radio astronomy produces shape parameters
/// (major axis FWHM, minor axis FWHM, position angle) in world coordinates
/// (radians on the sky) or in pixel coordinates (pixels, degrees measured
/// from pixel +x axis).  Analysis tools such as CASA's `imfit`
/// task need to convert between the two representations.
///
/// The conversions use the local Jacobian of the direction coordinate's
/// pixel<->world transform evaluated at the source centre position.  For
/// the common TAN projection this is the local pixel scale and sky rotation;
/// for other projections it accounts for the non-linear distortion.
///
/// Both functions accept and return a std::tuple of
/// (major, minor, position_angle), preserving units consistent with the
/// input domain:
/// <dl>
///   <dt>World domain</dt>  <dd>major_rad, minor_rad in radians; pa_rad in radians (N through
///   E).</dd> <dt>Pixel domain</dt>  <dd>major_pix, minor_pix in pixels; pa_pix in radians (pixel
///   +x axis).</dd>
/// </dl>
///
///
/// @par Example
/// @code{.cpp}
///   DirectionCoordinate coord = cs.direction_coordinate();
///   std::vector<double> pix_center = {128.0, 128.0};
///
///   // Fitted world-domain Gaussian
///   double major_rad = 3.0 * M_PI / (180.0 * 3600.0);  // 3 arcsec
///   double minor_rad = 1.5 * M_PI / (180.0 * 3600.0);  // 1.5 arcsec
///   double pa_rad    = 0.5;                              // ~29 deg N through E
///
///   auto [maj_pix, min_pix, pa_pix] =
///       gaussian_world_to_pixel(major_rad, minor_rad, pa_rad, coord, pix_center);
///
///   // Restore world parameters from pixel domain
///   auto [maj2, min2, pa2] =
///       gaussian_pixel_to_world(maj_pix, min_pix, pa_pix, coord, pix_center);
///
///   // Round-trip should recover the original values to floating-point precision
/// @endcode
///
///
/// @par Motivation
/// The Gaussian shape conversion is algebraically straightforward for
/// orthogonal projections but requires careful handling of the position
/// angle rotation when pixel axes are not aligned with world axes.
/// Encapsulating it in a dedicated header avoids duplicating the Jacobian
/// logic across image analysis tools.
///

/// Convert a Gaussian source shape from world to pixel coordinates.
///
///
/// Uses the local Jacobian of `coord`'s pixel-to-world transform
/// evaluated at `pixel` to transform the ellipse parameters.
/// The position angle is measured North through East in world space and is
/// rotated into the pixel frame accounting for the local sky orientation.
///
///
/// @param major_rad  Major axis FWHM in radians.
/// @param minor_rad  Minor axis FWHM in radians.
/// @param pa_rad     Position angle in radians (North through East).
/// @param coord      The direction coordinate defining the pixel<->world mapping.
/// @param pixel      Pixel position of the source centre [x_pix, y_pix].
/// @return Tuple of (major_pix, minor_pix, pa_pix) in pixel coordinates.
[[nodiscard]] std::tuple<double, double, double>
gaussian_world_to_pixel(double major_rad, double minor_rad, double pa_rad,
                        const DirectionCoordinate& coord, const std::vector<double>& pixel);

/// Convert a Gaussian source shape from pixel to world coordinates.
///
///
/// Inverse of `gaussian_world_to_pixel`.  Uses the world-to-pixel
/// Jacobian to map pixel-domain ellipse parameters back to the world frame.
/// The position angle in the returned tuple is measured North through East.
///
///
/// @param major_pix  Major axis FWHM in pixels.
/// @param minor_pix  Minor axis FWHM in pixels.
/// @param pa_pix     Position angle in radians (pixel +x axis orientation).
/// @param coord      The direction coordinate defining the pixel<->world mapping.
/// @param pixel      Pixel position of the source centre [x_pix, y_pix].
/// @return Tuple of (major_rad, minor_rad, pa_rad) in world coordinates.
[[nodiscard]] std::tuple<double, double, double>
gaussian_pixel_to_world(double major_pix, double minor_pix, double pa_pix,
                        const DirectionCoordinate& coord, const std::vector<double>& pixel);

} // namespace casacore_mini
