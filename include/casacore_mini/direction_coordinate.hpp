#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/projection.hpp"

#include <memory>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Direction coordinate using WCSLIB for celestial projections.

/// A celestial direction coordinate (RA/Dec, Gal lon/lat, etc.) using WCSLIB
/// for the non-linear projection stage.
///
/// Internally allocates a WCSLIB `wcsprm` structure for the projection math.
class DirectionCoordinate : public Coordinate {
  public:
    /// Construct a direction coordinate.
    /// @param ref  Direction reference frame (e.g. DirectionRef::j2000).
    /// @param proj  WCS projection (e.g. SIN, TAN).
    /// @param ref_lon_rad  Reference longitude in radians.
    /// @param ref_lat_rad  Reference latitude in radians.
    /// @param inc_lon_rad  Longitude increment in radians.
    /// @param inc_lat_rad  Latitude increment in radians.
    /// @param pc  2x2 PC matrix in row-major order (identity if empty).
    /// @param crpix_x  Reference pixel x (0-based).
    /// @param crpix_y  Reference pixel y (0-based).
    /// @param longpole_deg  Long pole in degrees (default 180).
    /// @param latpole_deg  Lat pole in degrees (default 0).
    DirectionCoordinate(DirectionRef ref, Projection proj, double ref_lon_rad, double ref_lat_rad,
                        double inc_lon_rad, double inc_lat_rad, std::vector<double> pc,
                        double crpix_x, double crpix_y, double longpole_deg = 180.0,
                        double latpole_deg = 0.0);

    ~DirectionCoordinate() override;

    // Move-only (WCSLIB resources).
    DirectionCoordinate(DirectionCoordinate&& other) noexcept;
    DirectionCoordinate& operator=(DirectionCoordinate&& other) noexcept;
    DirectionCoordinate(const DirectionCoordinate& other);
    DirectionCoordinate& operator=(const DirectionCoordinate& other);

    [[nodiscard]] CoordinateType type() const override {
        return CoordinateType::direction;
    }
    [[nodiscard]] std::size_t n_pixel_axes() const override {
        return 2;
    }
    [[nodiscard]] std::size_t n_world_axes() const override {
        return 2;
    }

    [[nodiscard]] std::vector<std::string> world_axis_names() const override;
    [[nodiscard]] std::vector<std::string> world_axis_units() const override;
    [[nodiscard]] std::vector<double> reference_value() const override;
    [[nodiscard]] std::vector<double> reference_pixel() const override;
    [[nodiscard]] std::vector<double> increment() const override;

    /// pixel -> [lon_rad, lat_rad].
    [[nodiscard]] std::vector<double> to_world(const std::vector<double>& pixel) const override;
    /// [lon_rad, lat_rad] -> pixel.
    [[nodiscard]] std::vector<double> to_pixel(const std::vector<double>& world) const override;

    [[nodiscard]] Record save() const override;
    [[nodiscard]] std::unique_ptr<Coordinate> clone() const override;

    [[nodiscard]] static std::unique_ptr<DirectionCoordinate> from_record(const Record& rec);

    [[nodiscard]] DirectionRef ref_frame() const noexcept {
        return ref_;
    }
    [[nodiscard]] const Projection& projection() const noexcept {
        return proj_;
    }

  private:
    struct WcsData;
    DirectionRef ref_;
    Projection proj_;
    double ref_lon_rad_;
    double ref_lat_rad_;
    double inc_lon_rad_;
    double inc_lat_rad_;
    std::vector<double> pc_;
    double crpix_x_;
    double crpix_y_;
    double longpole_deg_;
    double latpole_deg_;
    std::unique_ptr<WcsData> wcs_data_;

    void init_wcs();
};

} // namespace casacore_mini
