#pragma once

#include "casacore_mini/measure_types.hpp"

#include <array>

namespace casacore_mini {

/// @file
/// @brief UVW rotation, parallactic angle, and earth magnetic field utilities.

/// Compute 3x3 rotation matrix to transform UVW coordinates from one phase
/// center (ra1, dec1) to another (ra2, dec2). All angles in radians.
///
/// This implements the standard radio interferometry UVW rotation:
///   R = Rz(-ra2) Ry(dec2) Ry(-dec1) Rz(ra1)
/// simplified to the direct trigonometric form.
[[nodiscard]] std::array<std::array<double, 3>, 3> uvw_rotation_matrix(double ra1, double dec1,
                                                                       double ra2, double dec2);

/// Apply a 3x3 rotation matrix to a UVW vector.
[[nodiscard]] UvwValue rotate_uvw(const std::array<std::array<double, 3>, 3>& matrix,
                                  const UvwValue& uvw);

/// Compute the parallactic angle for a source at given hour angle and
/// declination, observed from a given geodetic latitude. All in radians.
///
/// Uses ERFA eraHd2pa: pa = atan2(sin(ha)*cos(lat),
///   cos(dec)*sin(lat) - sin(dec)*cos(lat)*cos(ha)).
[[nodiscard]] double parallactic_angle(double ha_rad, double dec_rad, double lat_rad);

/// Compute the Earth magnetic field vector. Not implemented (requires IGRF model).
/// @throws std::runtime_error always.
[[noreturn]] void earth_magnetic_field(double lon_rad, double lat_rad, double height_m,
                                       double epoch_yr);

/// Stateful UVW conversion helper.
class UvwMachine {
  public:
    UvwMachine(double from_ra_rad, double from_dec_rad, double to_ra_rad, double to_dec_rad);

    [[nodiscard]] const std::array<std::array<double, 3>, 3>& rotation_matrix() const noexcept {
        return rotation_matrix_;
    }
    [[nodiscard]] UvwValue convert(const UvwValue& uvw) const;

  private:
    std::array<std::array<double, 3>, 3> rotation_matrix_{};
};

/// Stateful parallactic-angle helper for a fixed observer latitude.
class ParAngleMachine {
  public:
    explicit ParAngleMachine(double latitude_rad) noexcept : latitude_rad_(latitude_rad) {}

    [[nodiscard]] double compute(double hour_angle_rad, double dec_rad) const;

  private:
    double latitude_rad_ = 0.0;
};

/// Stateful earth magnetic field helper for a fixed epoch.
class EarthMagneticMachine {
  public:
    explicit EarthMagneticMachine(double epoch_yr) noexcept : epoch_yr_(epoch_yr) {}

    [[noreturn]] void compute(double lon_rad, double lat_rad, double height_m) const;

  private:
    double epoch_yr_ = 0.0;
};

} // namespace casacore_mini
