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

/// Discriminator for coordinate types.
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

/// Abstract base class for all coordinate types.
///
/// Each concrete coordinate implements pixel<->world transforms and
/// Record serialization. Coordinates are owned via unique_ptr in
/// CoordinateSystem.
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

} // namespace casacore_mini
