#pragma once

#include "casacore_mini/coordinate.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Stokes coordinate: discrete pixel-to-Stokes-parameter mapping.

/// Stokes coordinate: maps integer pixel indices to Stokes parameter codes.
///
/// Stokes codes follow the FITS convention: I=1, Q=2, U=3, V=4, RR=5, RL=6, LR=7, LL=8,
/// XX=9, XY=10, YX=11, YY=12, ...
class StokesCoordinate : public Coordinate {
  public:
    /// Construct from a list of Stokes parameter codes.
    explicit StokesCoordinate(std::vector<std::int32_t> stokes_values);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::stokes;
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

    /// pixel -> Stokes code (returns the Stokes code as a double).
    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const override;
    /// Stokes code -> pixel index.
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const override;

    [[nodiscard]] Record save() const override;
    [[nodiscard]] std::unique_ptr<Coordinate> clone() const override;

    /// Restore from Record.
    [[nodiscard]] static std::unique_ptr<StokesCoordinate> from_record(const Record& rec);

    /// Access Stokes codes.
    [[nodiscard]] const std::vector<std::int32_t>& stokes_values() const noexcept {
        return stokes_;
    }

  private:
    std::vector<std::int32_t> stokes_;
};

} // namespace casacore_mini
