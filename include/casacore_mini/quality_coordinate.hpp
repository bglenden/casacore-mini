#pragma once

#include "casacore_mini/coordinate.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Quality coordinate: discrete integer mapping for data/error quality axis.

/// Quality coordinate: maps pixel indices to quality codes.
/// Similar to StokesCoordinate but for data quality (DATA=0, ERROR=1, etc.).
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
