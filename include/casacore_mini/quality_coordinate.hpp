// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Quality coordinate: discrete integer mapping for data/error quality axis.

/// <summary>
/// Single-axis coordinate that maps integer pixel indices to data-quality
/// codes (DATA, ERROR, etc.) for images carrying quality-plane information.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> Coordinate — abstract base class
///   <li> StokesCoordinate — analogous design for the polarization axis
/// </prerequisite>
///
/// <synopsis>
/// QualityCoordinate is structurally analogous to StokesCoordinate but
/// serves a different semantic purpose: it labels image planes that carry
/// different quality categories of the same data (e.g. calibrated data,
/// noise estimates, weights, flags).
///
/// Each pixel index along the quality axis maps to an integer quality code
/// stored in the <src>quality_values</src> list supplied at construction.
/// Conventional codes are:
///
/// <srcblock>
///   DATA  = 0   (calibrated data values)
///   ERROR = 1   (associated noise / uncertainty estimates)
/// </srcblock>
///
/// Additional codes may be used by application-specific conventions.
///
/// The forward transform to_world() performs a direct array lookup, returning
/// the quality code as a double.  The inverse transform to_pixel() performs a
/// linear search for the requested code.
///
/// The world axis name is "Quality" and the unit is an empty string.
/// </synopsis>
///
/// <example>
/// Create a two-plane quality axis carrying calibrated data and its noise:
/// <srcblock>
///   using namespace casacore_mini;
///
///   // Codes: DATA=0, ERROR=1
///   QualityCoordinate qual({0, 1});
///
///   // Pixel 0 -> DATA plane (code 0.0)
///   auto world = qual.to_world({0.0}); // world[0] == 0.0
///
///   // Pixel 1 -> ERROR plane (code 1.0)
///   world = qual.to_world({1.0});      // world[0] == 1.0
///
///   // Inverse: which pixel is the ERROR plane?
///   auto pixel = qual.to_pixel({1.0}); // pixel[0] == 1.0
/// </srcblock>
/// </example>
class QualityCoordinate : public Coordinate {
  public:
    /// Construct from a list of quality codes.
    explicit QualityCoordinate(std::vector<std::int32_t> quality_values);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::quality;
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

    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const override;
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const override;

    [[nodiscard]] Record save() const override;
    [[nodiscard]] std::unique_ptr<Coordinate> clone() const override;

    [[nodiscard]] static std::unique_ptr<QualityCoordinate> from_record(const Record& rec);

    [[nodiscard]] const std::vector<std::int32_t>& quality_values() const noexcept {
        return quality_;
    }

  private:
    std::vector<std::int32_t> quality_;
};

} // namespace casacore_mini
