// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/linear_xform.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief General N-dimensional linear coordinate.

/// <summary>
/// General N-dimensional linear coordinate using the LinearXform affine
/// transform machinery.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> Coordinate — abstract base class
///   <li> LinearXform — encapsulates crval, crpix, cdelt, and the PC matrix
/// </prerequisite>
///
/// <synopsis>
/// LinearCoordinate implements the standard FITS linear coordinate model for
/// an arbitrary number of axes N.  The pixel-to-world transform is:
///
/// <srcblock>
///   world[i] = crval[i] + cdelt[i] * sum_j( PC[i][j] * (pixel[j] - crpix[j]) )
/// </srcblock>
///
/// where crval is the reference world value vector, crpix is the reference
/// pixel vector, cdelt is the increment vector, and PC is the N×N rotation
/// and shear matrix (identity for unrotated axes).
///
/// All parameters are encapsulated in a LinearXform object supplied at
/// construction.  World axis names and units are stored as separate string
/// vectors.
///
/// LinearCoordinate is used for generic physical axes that do not require
/// the non-linear treatment of DirectionCoordinate (sky projections) or
/// the discrete-lookup treatment of StokesCoordinate.  Typical uses include
/// velocity axes, baseline axes in uv-plane images, or any set of axes that
/// are related to pixel coordinates by a constant affine transform.
/// </synopsis>
///
/// <example>
/// Construct a 1D velocity coordinate with 1 km/s channels:
/// <srcblock>
///   using namespace casacore_mini;
///
///   LinearXform xform;
///   xform.crval = {0.0};        // 0 km/s at reference pixel
///   xform.crpix = {255.0};      // reference pixel (0-based)
///   xform.cdelt = {1000.0};     // 1 km/s in m/s
///   xform.pc    = {1.0};        // 1x1 identity

///
///   LinearCoordinate vel({"Velocity"}, {"m/s"}, xform);
///
///   // Channel 256 -> 1000 m/s
///   auto world = vel.to_world({256.0}); // world[0] == 1000.0
/// </srcblock>
///
/// Construct a 2D uv-plane coordinate:
/// <srcblock>
///   LinearXform xform2;
///   xform2.crval = {0.0, 0.0};
///   xform2.crpix = {128.0, 128.0};
///   xform2.cdelt = {1.0, 1.0};       // 1 lambda per pixel
///   xform2.pc    = {1.0, 0.0, 0.0, 1.0}; // 2x2 identity
///
///   LinearCoordinate uv({"U", "V"}, {"lambda", "lambda"}, xform2);
/// </srcblock>
/// </example>
class LinearCoordinate : public Coordinate {
  public:
    /// Construct a linear coordinate.
    /// @param names  World axis names.
    /// @param units  World axis units.
    /// @param xform  The linear transform parameters.
    LinearCoordinate(std::vector<std::string> names, std::vector<std::string> units,
                     LinearXform xform);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::linear;
    }
    [[nodiscard]] std::size_t n_pixel_axes() const override {
        return xform_.n_axes();
    }
    [[nodiscard]] std::size_t n_world_axes() const override {
        return xform_.n_axes();
    }

    [[nodiscard]] std::vector<std::string> world_axis_names() const override {
        return names_;
    }
    [[nodiscard]] std::vector<std::string> world_axis_units() const override {
        return units_;
    }
    [[nodiscard]] std::vector<double> reference_value() const override {
        return xform_.crval;
    }
    [[nodiscard]] std::vector<double> reference_pixel() const override {
        return xform_.crpix;
    }
    [[nodiscard]] std::vector<double> increment() const override {
        return xform_.cdelt;
    }

    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const override;
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const override;

    [[nodiscard]] Record save() const override;
    [[nodiscard]] std::unique_ptr<Coordinate> clone() const override;

    [[nodiscard]] static std::unique_ptr<LinearCoordinate> from_record(const Record& rec);

  private:
    std::vector<std::string> names_;
    std::vector<std::string> units_;
    LinearXform xform_;
};

} // namespace casacore_mini
