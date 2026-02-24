#pragma once

#include "casacore_mini/coordinate.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Tabular coordinate: table-lookup with linear interpolation.

/// A tabular coordinate that maps between pixel and world via lookup tables
/// with linear interpolation.
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
