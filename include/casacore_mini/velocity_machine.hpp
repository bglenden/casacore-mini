#pragma once

#include "casacore_mini/measure_types.hpp"

namespace casacore_mini {

/// @file
/// @brief Interconversion of frequency, velocity, and Doppler measures.

/// Speed of light in m/s.
constexpr double kSpeedOfLight = 299792458.0;

/// Convert a Doppler ratio to velocity in m/s using the given Doppler convention.
///
/// Doppler conventions (DopplerRef):
/// - radio:   v = c * (1 - f/f0), so doppler = 1 - f/f0, v = c * doppler
/// - z:       v = c * z (optical, non-relativistic), z = f0/f - 1
/// - ratio:   ratio = f/f0 directly
/// - beta:    v = c * beta (relativistic, beta = v/c)
/// - gamma:   gamma = 1/sqrt(1 - beta^2)

/// Convert radio Doppler value to velocity (m/s).
/// radio_val = 1 - f/f0, so v = c * radio_val.
[[nodiscard]] double radio_doppler_to_velocity(double radio_val);

/// Convert velocity (m/s) to radio Doppler value.
[[nodiscard]] double velocity_to_radio_doppler(double velocity);

/// Convert optical/Z Doppler to velocity (m/s).
/// z = f0/f - 1, v_optical = c * z (non-relativistic approximation).
[[nodiscard]] double z_doppler_to_velocity(double z_val);

/// Convert velocity (m/s) to Z Doppler.
[[nodiscard]] double velocity_to_z_doppler(double velocity);

/// Convert relativistic beta (v/c) to velocity (m/s).
[[nodiscard]] double beta_to_velocity(double beta);

/// Convert velocity (m/s) to relativistic beta.
[[nodiscard]] double velocity_to_beta(double velocity);

/// Convert frequency ratio (f/f0) to radio velocity given rest frequency.
[[nodiscard]] double freq_ratio_to_radio_velocity(double freq_hz, double rest_freq_hz);

/// Convert radio velocity to frequency given rest frequency.
[[nodiscard]] double radio_velocity_to_freq(double velocity, double rest_freq_hz);

/// Convert frequency ratio to optical (Z) velocity given rest frequency.
[[nodiscard]] double freq_ratio_to_optical_velocity(double freq_hz, double rest_freq_hz);

/// Convert optical velocity to frequency given rest frequency.
[[nodiscard]] double optical_velocity_to_freq(double velocity, double rest_freq_hz);

/// Convert between Doppler types: radio ↔ Z ↔ beta ↔ gamma.
/// @param value    The Doppler value in the source convention.
/// @param from     Source Doppler convention (as DopplerRef enum underlying value).
/// @param to       Target Doppler convention.
/// @return The Doppler value in the target convention.

/// Convert radio Doppler to Z Doppler.
[[nodiscard]] double radio_to_z(double radio_val);

/// Convert Z Doppler to radio Doppler.
[[nodiscard]] double z_to_radio(double z_val);

/// Convert radio Doppler to beta (relativistic).
[[nodiscard]] double radio_to_beta(double radio_val);

/// Convert beta to radio Doppler.
[[nodiscard]] double beta_to_radio(double beta);

/// Convert Doppler values between conventions.
/// @throws std::invalid_argument for unsupported/invalid values.
[[nodiscard]] double convert_doppler(double value, DopplerRef from, DopplerRef to);

/// Stateful Doppler conversion helper, similar in role to casacore's
/// `MDoppler::Convert` helper objects.
class VelocityMachine {
  public:
    VelocityMachine(DopplerRef from_ref, DopplerRef to_ref) noexcept
        : from_ref_(from_ref), to_ref_(to_ref) {}

    [[nodiscard]] double convert(double value) const;
    [[nodiscard]] DopplerRef from_ref() const noexcept {
        return from_ref_;
    }
    [[nodiscard]] DopplerRef to_ref() const noexcept {
        return to_ref_;
    }

  private:
    DopplerRef from_ref_;
    DopplerRef to_ref_;
};

} // namespace casacore_mini
