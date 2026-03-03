// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/velocity_machine.hpp"

#include <cmath>
#include <stdexcept>

namespace casacore_mini {

double radio_doppler_to_velocity(double radio_val) {
    return kSpeedOfLight * radio_val;
}

double velocity_to_radio_doppler(double velocity) {
    return velocity / kSpeedOfLight;
}

double z_doppler_to_velocity(double z_val) {
    return kSpeedOfLight * z_val;
}

double velocity_to_z_doppler(double velocity) {
    return velocity / kSpeedOfLight;
}

double beta_to_velocity(double beta) {
    return kSpeedOfLight * beta;
}

double velocity_to_beta(double velocity) {
    return velocity / kSpeedOfLight;
}

double freq_ratio_to_radio_velocity(double freq_hz, double rest_freq_hz) {
    return kSpeedOfLight * (1.0 - freq_hz / rest_freq_hz);
}

double radio_velocity_to_freq(double velocity, double rest_freq_hz) {
    return rest_freq_hz * (1.0 - velocity / kSpeedOfLight);
}

double freq_ratio_to_optical_velocity(double freq_hz, double rest_freq_hz) {
    return kSpeedOfLight * (rest_freq_hz / freq_hz - 1.0);
}

double optical_velocity_to_freq(double velocity, double rest_freq_hz) {
    return rest_freq_hz / (1.0 + velocity / kSpeedOfLight);
}

double radio_to_z(double radio_val) {
    // radio = 1 - f/f0, so f/f0 = 1 - radio
    // z = f0/f - 1 = 1/(1 - radio) - 1
    double freq_ratio = 1.0 - radio_val;
    return 1.0 / freq_ratio - 1.0;
}

double z_to_radio(double z_val) {
    // z = f0/f - 1, so f/f0 = 1/(1+z)
    // radio = 1 - f/f0 = 1 - 1/(1+z) = z/(1+z)
    return z_val / (1.0 + z_val);
}

double radio_to_beta(double radio_val) {
    // radio velocity v_r = c * radio_val
    // Relativistic: v_r corresponds to f/f0 = 1 - radio_val
    // True velocity from relativistic Doppler: beta = ((1+z)^2 - 1) / ((1+z)^2 + 1)
    double z = radio_to_z(radio_val);
    double zp1_sq = (1.0 + z) * (1.0 + z);
    return (zp1_sq - 1.0) / (zp1_sq + 1.0);
}

double beta_to_radio(double beta) {
    // From beta, get z: (1+z)^2 = (1+beta)/(1-beta)
    double zp1_sq = (1.0 + beta) / (1.0 - beta);
    double z = std::sqrt(zp1_sq) - 1.0;
    return z_to_radio(z);
}

namespace {

double doppler_to_beta(double value, DopplerRef ref) {
    switch (ref) {
    case DopplerRef::radio:
        return radio_to_beta(value);
    case DopplerRef::z:
        return radio_to_beta(z_to_radio(value));
    case DopplerRef::ratio:
        if (value <= 0.0) {
            throw std::invalid_argument("Doppler ratio must be > 0");
        }
        return radio_to_beta(1.0 - value);
    case DopplerRef::beta:
        return value;
    case DopplerRef::gamma:
        if (value < 1.0) {
            throw std::invalid_argument("Doppler gamma must be >= 1");
        }
        return std::sqrt(1.0 - 1.0 / (value * value));
    }
    throw std::invalid_argument("Unknown DopplerRef in doppler_to_beta");
}

double beta_to_doppler(double beta, DopplerRef ref) {
    switch (ref) {
    case DopplerRef::radio:
        return beta_to_radio(beta);
    case DopplerRef::z:
        return radio_to_z(beta_to_radio(beta));
    case DopplerRef::ratio:
        return 1.0 - beta_to_radio(beta);
    case DopplerRef::beta:
        return beta;
    case DopplerRef::gamma:
        return 1.0 / std::sqrt(1.0 - beta * beta);
    }
    throw std::invalid_argument("Unknown DopplerRef in beta_to_doppler");
}

} // namespace

double convert_doppler(double value, DopplerRef from, DopplerRef to) {
    if (from == to) {
        return value;
    }
    return beta_to_doppler(doppler_to_beta(value, from), to);
}

double VelocityMachine::convert(double value) const {
    return convert_doppler(value, from_ref_, to_ref_);
}

} // namespace casacore_mini
