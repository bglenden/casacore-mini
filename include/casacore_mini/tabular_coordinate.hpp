// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Tabular coordinate: table-lookup with linear interpolation.

///
/// Single-axis coordinate that maps between pixel and world values via an
/// explicit lookup table with linear interpolation between entries.
///
///
///
///
/// @par Prerequisites
///   - Coordinate — abstract base class
///
///
///
/// TabularCoordinate is appropriate when the pixel-to-world mapping cannot
/// be expressed as a simple linear or non-linear analytic formula.  Instead,
/// a pair of parallel arrays — `pixel_values` and
/// `world_values` — defines the mapping at a discrete set of
/// sample points.  Between sample points the mapping is linearly
/// interpolated.
///
/// Requirements on the input arrays:
///   - Both arrays must have the same length (at least 2 entries).
///   - `pixel_values` must be strictly monotonically increasing so
///     that the inverse transform (world → pixel) can be computed by binary
///     search followed by linear interpolation.
///   - `world_values` may be non-monotonic, but the inverse
///     transform is only well-defined if it is also monotonic.
///
/// The reference value returned by reference_value() is the world value at
/// the first table entry.  The reference pixel is the first pixel value.
/// The increment() is computed as the mean step size across the table.
///
/// Typical uses include irregularly sampled time axes, non-linear frequency
/// solutions, or any 1D axis whose values are known only at observed
/// sample positions.
///
///
/// @par Example
/// Map five unevenly-spaced time stamps to their MJD values:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // Pixel indices of the time samples
///   std::vector<double> pixels = {0.0, 1.0, 2.0, 3.0, 4.0};
///
///   // Corresponding MJD values (not uniformly spaced)
///   std::vector<double> mjd = {
///       58000.0, 58000.5, 58001.2, 58002.0, 58003.9
///   };
///
///   TabularCoordinate time(pixels, mjd, "Time", "d");
///
///   // Interpolate at pixel 1.5 -> MJD between 58000.5 and 58001.2
///   auto world = time.to_world({1.5}); // world[0] ~ 58000.85
///
///   // Inverse: which pixel corresponds to MJD 58002.0?
///   auto pixel = time.to_pixel({58002.0}); // pixel[0] == 3.0
/// @endcode
///
class TabularCoordinate : public Coordinate {
  public:
    /// Construct from pixel and world value arrays.
    /// @param pixel_values  Pixel coordinates (must be monotonically increasing).
    /// @param world_values  Corresponding world coordinates.
    /// @param name  World axis name.
    /// @param unit  World axis unit.
    TabularCoordinate(std::vector<double> pixel_values, std::vector<double> world_values,
                      std::string name, std::string unit);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::tabular;
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

    [[nodiscard]] static std::unique_ptr<TabularCoordinate> from_record(const Record& rec);

  private:
    std::vector<double> pixel_values_;
    std::vector<double> world_values_;
    std::string name_;
    std::string unit_;
};

} // namespace casacore_mini
