#pragma once

#include "casacore_mini/direction_coordinate.hpp"

#include <tuple>

namespace casacore_mini {

/// @file
/// @brief Convert Gaussian source shape parameters between direction coordinate frames.

/// Convert Gaussian source shape parameters using the Jacobian of a direction
/// coordinate's pixel<->world transform.
///
/// @param major_rad  Major axis FWHM in radians.
/// @param minor_rad  Minor axis FWHM in radians.
/// @param pa_rad  Position angle in radians (N through E).
/// @param coord  The direction coordinate at the source position.
/// @param pixel  The pixel position of the source center [x, y].
/// @return  Tuple of (major_pix, minor_pix, pa_pix) in pixel coordinates.
[[nodiscard]] std::tuple<double, double, double>
gaussian_world_to_pixel(double major_rad, double minor_rad, double pa_rad,
                        const DirectionCoordinate& coord, const std::vector<double>& pixel);

/// Inverse: convert Gaussian from pixel to world coordinates.
[[nodiscard]] std::tuple<double, double, double>
gaussian_pixel_to_world(double major_pix, double minor_pix, double pa_pix,
                        const DirectionCoordinate& coord, const std::vector<double>& pixel);

} // namespace casacore_mini
