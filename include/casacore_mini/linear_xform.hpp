#pragma once

#include <cstddef>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Linear WCS transform: world = crval + cdelt * pc * (pixel - crpix).

/// A linear WCS transformation stage.
///
/// Implements the standard WCS linear mapping between pixel and world coordinates:
///   world[i] = crval[i] + sum_j( cdelt[i] * pc[i*n+j] * (pixel[j] - crpix[j]) )
///
/// Inverse (world to pixel) uses matrix inversion of the pc*cdelt product matrix.
struct LinearXform {
    /// Reference pixel coordinates (0-based).
    std::vector<double> crpix;
    /// Coordinate increments.
    std::vector<double> cdelt;
    /// Reference world coordinates.
    std::vector<double> crval;
    /// PC matrix in row-major order (size n*n where n = crpix.size()).
    /// Identity matrix if empty.
    std::vector<double> pc;

    /// Number of axes.
    [[nodiscard]] std::size_t n_axes() const noexcept {
        return crpix.size();
    }

    /// Forward transform: pixel coordinates to world coordinates.
    /// @param pixel  Input pixel coordinates (size must equal n_axes()).
    /// @return World coordinates.
    /// @throws std::invalid_argument if pixel size != n_axes().
    [[nodiscard]] std::vector<double> pixel_to_world(const std::vector<double>& pixel) const;

    /// Inverse transform: world coordinates to pixel coordinates.
    /// @param world  Input world coordinates (size must equal n_axes()).
    /// @return Pixel coordinates.
    /// @throws std::invalid_argument if world size != n_axes() or matrix is singular.
    [[nodiscard]] std::vector<double> world_to_pixel(const std::vector<double>& world) const;
};

} // namespace casacore_mini
