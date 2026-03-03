// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cstddef>
#include <optional>

namespace casacore_mini {

/// @file
/// @brief Factory functions and finders for coordinate types within a CoordinateSystem.
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

///
/// Utility functions for locating the index of specific coordinate sub-types
/// within a `CoordinateSystem`.  Each function scans the coordinate list for
/// the first entry of the requested type and returns its index, or
/// `std::nullopt` if no coordinate of that type exists.
///
/// These are helper wrappers around `CoordinateSystem::coordinate_type()` that
/// eliminate repetitive type-check loops in client code.
///

/// Find the index of the first DirectionCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_direction_coordinate(const CoordinateSystem& cs);

/// Find the index of the first SpectralCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_spectral_coordinate(const CoordinateSystem& cs);

/// Find the index of the first StokesCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_stokes_coordinate(const CoordinateSystem& cs);

/// @}
} // namespace casacore_mini
