// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/measure_types.hpp"

#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Spectral coordinate: linear frequency channel mapping.

/// 
/// Single-axis coordinate mapping pixel channel indices to physical
/// frequencies via a linear transform.
/// 
///
///
///
/// @par Prerequisites
///   - Coordinate — abstract base class
///   - FrequencyRef — enumeration of supported frequency reference frames
///         (LSRK, LSRD, BARY, TOPO, etc.)
/// 
///
/// 
/// SpectralCoordinate represents a single spectral (frequency) axis.  The
/// pixel-to-world mapping is purely linear:
///
/// @code{.cpp}
///   freq_hz = crval_hz + cdelt_hz * (pixel - crpix)
/// @endcode
///
/// where crval_hz is the reference frequency at reference pixel crpix, and
/// cdelt_hz is the channel width (increment).  The reference pixel is
/// 0-based, consistent with the rest of casacore-mini's pixel convention.
///
/// The coordinate carries a frequency reference frame (FrequencyRef) that
/// identifies the Doppler/kinematic frame in which the frequencies are
/// expressed (e.g. LSRK for radio spectral-line data).
///
/// An optional rest frequency may be stored for velocity-frame conversions.
/// When non-zero, it allows external code to compute radio or optical
/// velocities relative to a spectral line.
///
/// The world axis name is "Frequency" and the unit is "Hz".
/// 
///
/// @par Example
/// Construct a 1024-channel LSRK spectral axis centred on the HI 21-cm line:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   const double hi_rest_hz = 1.42040575177e9; // HI rest frequency in Hz
///   const double chan_width  = 10.0e3;          // 10 kHz per channel
///
///   SpectralCoordinate spec(
///       FrequencyRef::lsrk,
///       hi_rest_hz,       // crval: centre frequency at reference pixel
///       chan_width,        // cdelt: channel width in Hz
///       511.5,             // crpix: centre of a 1024-channel band (0-based)
///       hi_rest_hz         // rest frequency for velocity conversion
///   );
///
///   // Channel 511.5 should return the rest frequency
///   auto world = spec.to_world({511.5}); // world[0] ~ 1.42040575177e9 Hz
///
///   // Inverse: which channel corresponds to 1.421 GHz?
///   auto pixel = spec.to_pixel({1.421e9});
/// @endcode
/// 
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
