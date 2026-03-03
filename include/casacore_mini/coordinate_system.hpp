// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/obs_info.hpp"
#include "casacore_mini/record.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief CoordinateSystem: composition of multiple Coordinate objects with axis mapping.
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

///
/// Composition of multiple Coordinate objects with pixel/world axis mapping.
///
///
///
///
/// @par Prerequisites
///   - Coordinate — abstract coordinate base class
///   - DirectionCoordinate, SpectralCoordinate, StokesCoordinate, etc.
///   - ObsInfo — telescope and date of observation
///
///
///
/// CoordinateSystem describes the complete mapping between pixel axes
/// (integer lattice coordinates) and world axes (physical coordinates
/// such as right ascension, declination, frequency, and Stokes parameter).
///
/// A CoordinateSystem is built by adding one or more Coordinate objects
/// via add_coordinate().  Each coordinate contributes a set of pixel
/// and world axes; these are accumulated into system-level axis lists.
/// Removed axes use replacement values rather than live mappings.
///
/// The system supports forward (pixel → world) and inverse
/// (world → pixel) transforms via to_world() and to_pixel().  Both
/// methods delegate to the constituent Coordinate objects.
///
/// CoordinateSystem can be serialized to and restored from a Record
/// using the save() / restore() methods, which produce a Record
/// compatible with casacore's CoordinateSystem::save() format.
///
///
/// @par Example
/// Build a 2D sky + spectral coordinate system:
/// @code{.cpp}
///   using namespace casacore_mini;
///   CoordinateSystem cs;
///
///   // Direction (RA/Dec)
///   auto dir = std::make_unique<DirectionCoordinate>(...);
///   cs.add_coordinate(std::move(dir));
///
///   // Spectral
///   auto spec = std::make_unique<SpectralCoordinate>(...);
///   cs.add_coordinate(std::move(spec));
///
///   // Convert pixel {x,y,chan} to world {ra, dec, freq}
///   std::vector<double> pixel = {128.0, 128.0, 0.0};
///   auto world = cs.to_world(pixel);
/// @endcode
///
class CoordinateSystem {
  public:
    CoordinateSystem() = default;

    /// Add a coordinate to the system.
    /// Axes are appended in order. Returns the index of the added coordinate.
    std::size_t add_coordinate(std::unique_ptr<Coordinate> coord);

    /// Number of coordinates in the system.
    [[nodiscard]] std::size_t n_coordinates() const noexcept {
        return coords_.size();
    }
    /// Total number of pixel axes across all coordinates.
    [[nodiscard]] std::size_t n_pixel_axes() const noexcept {
        return total_pixel_;
    }
    /// Total number of world axes across all coordinates.
    [[nodiscard]] std::size_t n_world_axes() const noexcept {
        return total_world_;
    }

    /// Access a coordinate by index.
    [[nodiscard]] const Coordinate& coordinate(std::size_t index) const;

    /// Find the coordinate index containing a given world axis.
    /// Returns {coord_index, axis_within_coord} or nullopt.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    find_world_axis(std::size_t world_axis) const;

    /// Find the coordinate index containing a given pixel axis.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>>
    find_pixel_axis(std::size_t pixel_axis) const;

    /// Access/set observation info.
    [[nodiscard]] const ObsInfo& obs_info() const noexcept {
        return obs_info_;
    }
    void set_obs_info(ObsInfo info) {
        obs_info_ = std::move(info);
    }

    /// Convert pixel coordinates to world coordinates.
    /// The input vector must have n_pixel_axes() elements.
    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const;

    /// Convert world coordinates to pixel coordinates.
    /// The input vector must have n_world_axes() elements.
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const;

    /// Serialize to Record matching upstream CoordinateSystem::save format.
    [[nodiscard]] Record save() const;
    /// Restore from Record.
    [[nodiscard]] static CoordinateSystem restore(const Record& rec);

  private:
    struct CoordEntry {
        std::unique_ptr<Coordinate> coord;
        std::vector<std::int64_t> world_map; // system world axis for each coord world axis
        std::vector<std::int64_t> pixel_map; // system pixel axis for each coord pixel axis
        std::vector<double> world_replace;   // replacement values for removed world axes
        std::vector<double> pixel_replace;   // replacement values for removed pixel axes
    };

    std::vector<CoordEntry> coords_;
    std::size_t total_pixel_ = 0;
    std::size_t total_world_ = 0;
    ObsInfo obs_info_;
};

/// @}
} // namespace casacore_mini
