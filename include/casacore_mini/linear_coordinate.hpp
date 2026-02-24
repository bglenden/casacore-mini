#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/linear_xform.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief General N-dimensional linear coordinate.

/// A general linear coordinate using the LinearXform machinery.
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
