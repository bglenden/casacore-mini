// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Linear WCS transform: world = crval + cdelt * pc * (pixel - crpix).

///
/// A linear WCS transformation stage mapping pixel to world coordinates.
///
///
///
///
///
/// `LinearXform` implements the standard WCS affine mapping between pixel and
/// world coordinates as defined by Calabretta & Greisen (2002, A&A 395, 1077):
///
/// `world[i] = crval[i] + sum_j( cdelt[i] * pc[i*n+j] * (pixel[j] - crpix[j]) )`
///
/// All arrays are indexed in the same order as the FITS WCS keywords they
/// correspond to.  `crpix` is 0-based (not 1-based as in FITS).
///
/// The PC matrix is stored in row-major order with `n*n` elements where
/// `n = crpix.size()`.  An empty `pc` vector is treated as an identity matrix.
///
/// The inverse transform (`world_to_pixel`) solves the linear system by
/// computing the matrix inverse of `diag(cdelt) * pc`.  This fails if the
/// composite matrix is singular.
///
///
/// @par Example
/// @code{.cpp}
///   LinearXform xf;
///   xf.crpix = {255.5, 255.5};   // 0-based reference pixel
///   xf.cdelt = {-4.84813e-6, 4.84813e-6};  // radians/pixel
///   xf.crval = {1.4, 0.0};       // reference world coords (radians)
///   // pc defaults to identity
///   auto world = xf.pixel_to_world({300.0, 200.0});
/// @endcode
///
///
/// @par Motivation
/// Many coordinate types (Linear, Direction, Spectral) share the same affine
/// pixel-to-world mapping layer.  Factoring it into a standalone struct
/// avoids duplicating the matrix-inversion logic in each coordinate class
/// and simplifies round-trip testing.
///
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
