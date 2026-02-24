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

/// Find the index of the first DirectionCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_direction_coordinate(const CoordinateSystem& cs);

/// Find the index of the first SpectralCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_spectral_coordinate(const CoordinateSystem& cs);

/// Find the index of the first StokesCoordinate in the system.
[[nodiscard]] std::optional<std::size_t> find_stokes_coordinate(const CoordinateSystem& cs);

} // namespace casacore_mini
