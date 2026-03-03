// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Stokes coordinate: discrete pixel-to-Stokes-parameter mapping.
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

///
/// Single-axis coordinate that maps integer pixel indices to FITS Stokes
/// parameter codes (I, Q, U, V, RR, LL, etc.).
///
///
///
///
/// @par Prerequisites
///   - Coordinate — abstract base class
///   - FITS WCS Paper I (Greisen & Calabretta) — Stokes axis convention
///
///
///
/// StokesCoordinate represents a polarization axis whose world values are
/// not physical angles or frequencies but discrete enumeration codes
/// defined by the FITS standard:
///
/// @code{.cpp}
///   I=1  Q=2  U=3  V=4
///   RR=5 RL=6 LR=7 LL=8
///   XX=9 XY=10 YX=11 YY=12
///   ...
/// @endcode
///
/// Each pixel index maps to exactly one Stokes code stored in the
/// `stokes_values` list supplied at construction.  The world
/// representation returned by to_world() is the Stokes code cast to
/// double.
///
/// Unlike other coordinates there is no analytic formula relating pixel
/// to world: to_world() performs a direct array lookup, and to_pixel()
/// performs a linear search for the requested code.
///
/// The world axis name is "Stokes" and the unit is an empty string,
/// consistent with the FITS convention for the Stokes axis.
///
///
/// @par Example
/// Create a full-Stokes (I, Q, U, V) polarization axis:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // FITS Stokes codes: I=1, Q=2, U=3, V=4
///   StokesCoordinate stokes({1, 2, 3, 4});
///
///   // Pixel 0 -> Stokes I (code 1.0)
///   auto world = stokes.to_world({0.0}); // world[0] == 1.0
///
///   // Find the pixel index of Stokes V (code 4)
///   auto pixel = stokes.to_pixel({4.0}); // pixel[0] == 3.0
/// @endcode
///
/// Create a circular-feed correlation axis (RR, LL only):
/// @code{.cpp}
///   StokesCoordinate circ({5, 8}); // RR=5, LL=8
/// @endcode
///
class StokesCoordinate : public Coordinate {
  public:
    /// Construct from a list of Stokes parameter codes.
    explicit StokesCoordinate(std::vector<std::int32_t> stokes_values);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::stokes;
    }
    [[nodiscard]] std::size_t n_pixel_axes() const override {
        return 1;
    }
    [[nodiscard]] std::size_t n_world_axes() const override {
        return 1;
    }

    [[nodiscard]] std::vector<std::string> world_axis_names() const override;
    [[nodiscard]] std::vector<std::string> world_axis_units() const override;
    [[nodiscard]] std::vector<double> reference_value() const override;
    [[nodiscard]] std::vector<double> reference_pixel() const override;
    [[nodiscard]] std::vector<double> increment() const override;

    /// pixel -> Stokes code (returns the Stokes code as a double).
    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const override;
    /// Stokes code -> pixel index.
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const override;

    [[nodiscard]] Record save() const override;
    [[nodiscard]] std::unique_ptr<Coordinate> clone() const override;

    /// Restore from Record.
    [[nodiscard]] static std::unique_ptr<StokesCoordinate> from_record(const Record& rec);

    /// Access Stokes codes.
    [[nodiscard]] const std::vector<std::int32_t>& stokes_values() const noexcept {
        return stokes_;
    }

  private:
    std::vector<std::int32_t> stokes_;
};

/// @}
} // namespace casacore_mini
