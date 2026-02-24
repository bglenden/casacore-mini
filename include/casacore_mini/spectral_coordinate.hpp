#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/measure_types.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Spectral coordinate: linear frequency channel mapping.

/// A spectral coordinate mapping pixel channels to frequencies.
///
/// Uses a simple linear mapping: freq = crval + cdelt * (pixel - crpix).
/// Supports an optional rest frequency for velocity conversion.
class SpectralCoordinate : public Coordinate {
  public:
    /// Construct a spectral coordinate.
    /// @param ref_frame  Frequency reference frame (e.g. FrequencyRef::lsrk).
    /// @param crval_hz  Reference frequency in Hz.
    /// @param cdelt_hz  Channel width in Hz.
    /// @param crpix  Reference pixel (0-based).
    /// @param rest_freq_hz  Rest frequency in Hz (0 if not applicable).
    SpectralCoordinate(FrequencyRef ref_frame, double crval_hz, double cdelt_hz, double crpix,
                       double rest_freq_hz = 0.0);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::spectral;
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

    [[nodiscard]] static std::unique_ptr<SpectralCoordinate> from_record(const Record& rec);

    [[nodiscard]] FrequencyRef ref_frame() const noexcept {
        return ref_frame_;
    }
    [[nodiscard]] double rest_freq_hz() const noexcept {
        return rest_freq_hz_;
    }

  private:
    FrequencyRef ref_frame_;
    double crval_hz_;
    double cdelt_hz_;
    double crpix_;
    double rest_freq_hz_;
};

} // namespace casacore_mini
