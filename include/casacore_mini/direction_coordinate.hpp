// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

///
/// Celestial direction coordinate (longitude/latitude pair) using WCSLIB for
/// non-linear sky projections.
///
///
///
///
/// @par Prerequisites
///   - Coordinate — abstract base class defining the pixel/world interface
///   - DirectionRef — enumeration of supported celestial reference frames
///         (J2000, B1950, Galactic, etc.)
///   - Projection — WCS projection type (SIN, TAN, ZEA, etc.)
///   - WCSLIB — the underlying C library performing the non-linear math
///
///
///
/// DirectionCoordinate maps a pair of pixel axes (typically image x and y)
/// to a pair of angular world axes representing a celestial longitude and
/// latitude.  Common examples are right ascension and declination in the
/// J2000 frame, or Galactic longitude and latitude.
///
/// The mathematical model follows the FITS WCS standard (Greisen &
/// Calabretta 2002, A&A 395, 1077).  The coordinate is parameterized by:
///
///   - A reference frame (DirectionRef), which fixes the equatorial or
///     Galactic system and epoch.
///   - A sky projection (Projection), such as SIN (orthographic, used for
///     interferometric images) or TAN (gnomonic, used for optical fields).
///   - A reference longitude and latitude in radians (crval).
///   - A longitude and latitude increment in radians per pixel (cdelt).
///   - A 2×2 PC matrix for rotation and shear.
///   - Reference pixel coordinates in 0-based pixel space (crpix).
///   - Optional native longitude and latitude of the celestial pole
///     (longpole_deg, latpole_deg) that determine the orientation of the
///     native coordinate system relative to the celestial one.
///
/// Pixel-to-world transforms are delegated to WCSLIB via an internal
/// `wcsprm` structure allocated in the constructor and freed in the
/// destructor.  Copying performs a deep clone of the WCSLIB state.
///
/// The world axis ordering is [longitude, latitude], and units are radians
/// for both axes.
///
///
/// @par Example
/// Construct a J2000 RA/Dec coordinate with a SIN projection centred on the
/// Galactic centre, with 1 arcsecond pixels:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   const double deg = M_PI / 180.0;
///   const double arcsec = deg / 3600.0;
///
///   DirectionCoordinate dir(
///       DirectionRef::j2000,
///       Projection{ProjectionType::SIN},
///       266.405 * deg,   // ref RA  (rad)
///      -29.008 * deg,   // ref Dec (rad)
///      -1.0 * arcsec,   // RA increment (negative: RA increases right-to-left)
///       1.0 * arcsec,   // Dec increment
///       {},              // identity PC matrix
///       512.0,           // crpix x (0-based)
///       512.0            // crpix y (0-based)
///   );
///
///   // Forward transform: centre pixel -> (RA, Dec) in radians
///   auto world = dir.to_world({512.0, 512.0});
///   // world[0] == 266.405 * deg, world[1] == -29.008 * deg
///
///   // Inverse transform
///   auto pixel = dir.to_pixel(world);
/// @endcode
///
///
/// @par Motivation
/// Radio and optical images require accurate non-linear sky projections.
/// Delegating the projection math to WCSLIB avoids re-implementing a complex,
/// well-tested standard and keeps casacore-mini compatible with FITS files
/// produced by other packages.
///
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

/// @}
} // namespace casacore_mini
