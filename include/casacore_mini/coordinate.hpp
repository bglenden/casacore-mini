// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/record.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Abstract coordinate base class and CoordinateType enum.
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

///
/// Discriminator enumeration identifying the concrete type of a Coordinate.
///
///
///
///
///
/// Every concrete coordinate subclass carries one of these values, returned
/// by `Coordinate::type()`.  The value is used by the factory
/// function `Coordinate::restore()` to dispatch to the correct
/// derived-class constructor when deserializing from a Record.
///
/// The numeric values are stable across versions so that serialized Records
/// remain compatible.
///
enum class CoordinateType : std::uint8_t {
    linear,
    direction,
    spectral,
    stokes,
    tabular,
    quality,
};

/// Convert CoordinateType to its Record name prefix (e.g. "direction", "spectral").
[[nodiscard]] std::string coordinate_type_to_string(CoordinateType t);
/// Parse a coordinate type string. Case-insensitive.
/// @throws std::invalid_argument if unrecognized.
[[nodiscard]] CoordinateType string_to_coordinate_type(std::string_view s);

///
/// Abstract base class defining the pixel/world transform contract for all
/// coordinate types.
///
///
///
///
/// @par Prerequisites
///   - Record — the serialization container used by save()/restore()
///   - CoordinateType — discriminator returned by type()
///
///
///
/// Coordinate is the common interface shared by all concrete coordinate
/// classes: DirectionCoordinate, SpectralCoordinate, StokesCoordinate,
/// LinearCoordinate, TabularCoordinate, and QualityCoordinate.
///
/// Every concrete coordinate describes a bijective (or near-bijective)
/// mapping between a set of pixel axes (integer lattice positions) and a
/// set of world axes (physical quantities such as angle, frequency, or
/// polarization code).
///
/// The number of pixel axes and world axes need not be equal in the general
/// case, but all current concrete subclasses use square (N:N) mappings.
///
/// Concrete coordinates are normally held through
/// `std::unique_ptr<Coordinate>` inside a CoordinateSystem.  They
/// support deep-copy via clone() and round-trip serialization via save() and
/// restore().
///
/// Lifetime and ownership: instances are move-only in the sense that
/// CoordinateSystem takes ownership via unique_ptr.  Protected copy/move
/// special members are provided so that clone() implementations in derived
/// classes can delegate to the compiler-generated copy constructor.
///
///
/// @par Example
/// Typical usage through a derived class:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // Construct a concrete coordinate
///   SpectralCoordinate spec(FrequencyRef::lsrk,
///                           1.42e9,   // crval Hz
///                           1.0e6,    // cdelt Hz
///                           0.0);     // crpix (0-based)
///
///   // Query axes through the base-class interface
///   std::size_t np = spec.n_pixel_axes(); // 1
///   std::size_t nw = spec.n_world_axes(); // 1
///
///   // Forward transform
///   std::vector<double> world = spec.to_world({3.0}); // channel 3 in Hz
///
///   // Serialize / deserialize
///   Record rec = spec.save();
///   auto restored = Coordinate::restore(rec);
/// @endcode
///
///
/// @par Motivation
/// A common abstract interface allows CoordinateSystem to store heterogeneous
/// coordinate objects in a single container and to apply pixel/world
/// transforms uniformly without knowing the concrete type at the call site.
/// The type() discriminator is reserved for the factory and for code that
/// genuinely needs to downcast.
///
class Coordinate {
  public:
    virtual ~Coordinate() = default;

    /// The coordinate type discriminator.
    [[nodiscard]] virtual CoordinateType type() const = 0;

    /// Number of pixel axes.
    [[nodiscard]] virtual std::size_t n_pixel_axes() const = 0;
    /// Number of world axes.
    [[nodiscard]] virtual std::size_t n_world_axes() const = 0;

    /// World axis names (e.g. "Right Ascension", "Declination").
    [[nodiscard]] virtual std::vector<std::string> world_axis_names() const = 0;
    /// World axis units (e.g. "rad", "Hz").
    [[nodiscard]] virtual std::vector<std::string> world_axis_units() const = 0;

    /// Reference world values.
    [[nodiscard]] virtual std::vector<double> reference_value() const = 0;
    /// Reference pixel values.
    [[nodiscard]] virtual std::vector<double> reference_pixel() const = 0;
    /// Coordinate increments.
    [[nodiscard]] virtual std::vector<double> increment() const = 0;

    /// Forward transform: pixel to world.
    /// @param pixel  Pixel coordinates (size must equal n_pixel_axes()).
    /// @return World coordinates (size equals n_world_axes()).
    [[nodiscard]] virtual std::vector<double> to_world(const std::vector<double>& pixel) const = 0;
    /// Inverse transform: world to pixel.
    /// @param world  World coordinates (size must equal n_world_axes()).
    /// @return Pixel coordinates (size equals n_pixel_axes()).
    [[nodiscard]] virtual std::vector<double> to_pixel(const std::vector<double>& world) const = 0;

    /// Serialize to Record.
    [[nodiscard]] virtual Record save() const = 0;

    /// Deep copy.
    [[nodiscard]] virtual std::unique_ptr<Coordinate> clone() const = 0;

    /// Factory: restore a Coordinate from a Record.
    /// Dispatches on the coordinate type string in the record.
    /// @throws std::invalid_argument if type is unrecognized.
    [[nodiscard]] static std::unique_ptr<Coordinate> restore(const Record& rec);

    /// Restore with an explicit type hint.  If the record contains a
    /// ``coordinate_type`` field it is used; otherwise @p type_hint is used.
    /// This allows reading upstream casacore records which do not embed
    /// ``coordinate_type`` in the coordinate sub-records.
    [[nodiscard]] static std::unique_ptr<Coordinate> restore(const Record& rec,
                                                             CoordinateType type_hint);

  protected:
    Coordinate() = default;
    Coordinate(const Coordinate&) = default;
    Coordinate& operator=(const Coordinate&) = default;
    Coordinate(Coordinate&&) = default;
    Coordinate& operator=(Coordinate&&) = default;
};

/// @}
} // namespace casacore_mini
