#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MS utility functions: StokesConverter, MsDopplerUtil, MsHistoryHandler.

// ---------------------------------------------------------------------------
// StokesConverter
// ---------------------------------------------------------------------------

/// Convert between Stokes/correlation types.
///
/// Stokes codes follow the FITS convention:
/// I=1, Q=2, U=3, V=4, RR=5, RL=6, LR=7, LL=8, XX=9, XY=10, YX=11, YY=12.
class StokesConverter {
  public:
    /// Set up conversion from input to output Stokes types.
    /// @param in_types  Input correlation types (e.g. {5,6,7,8} for RR,RL,LR,LL).
    /// @param out_types  Desired output types (e.g. {1,2,3,4} for I,Q,U,V).
    StokesConverter(std::vector<std::int32_t> in_types,
                    std::vector<std::int32_t> out_types);

    /// Convert a visibility vector from input to output Stokes basis.
    ///
    /// @param in_data  Complex visibilities in input basis [n_in].
    /// @return  Visibilities in output basis [n_out].
    [[nodiscard]] std::vector<std::complex<float>>
    convert(const std::vector<std::complex<float>>& in_data) const;

    /// Number of input correlations.
    [[nodiscard]] std::size_t n_in() const noexcept { return in_types_.size(); }
    /// Number of output correlations.
    [[nodiscard]] std::size_t n_out() const noexcept { return out_types_.size(); }

  private:
    std::vector<std::int32_t> in_types_;
    std::vector<std::int32_t> out_types_;
};

// ---------------------------------------------------------------------------
// MsDopplerUtil
// ---------------------------------------------------------------------------

/// Doppler tracking utility for MS data.
///
/// Wraps VelocityMachine conversions in an MS context, providing
/// velocity-to-frequency and frequency-to-velocity conversions
/// using the rest frequency from the SOURCE subtable.
struct MsDopplerUtil {
    /// Convert radio velocity (m/s) to frequency (Hz) given rest frequency.
    [[nodiscard]] static double velocity_to_frequency(double velocity_mps,
                                                       double rest_freq_hz);

    /// Convert frequency (Hz) to radio velocity (m/s) given rest frequency.
    [[nodiscard]] static double frequency_to_velocity(double freq_hz,
                                                       double rest_freq_hz);
};

// ---------------------------------------------------------------------------
// MsHistoryHandler
// ---------------------------------------------------------------------------

/// Add entries to the HISTORY subtable of a MeasurementSet.
class MsHistoryHandler {
  public:
    explicit MsHistoryHandler(MeasurementSet& ms);

    /// Add a history entry.
    ///
    /// @param message  Log message.
    /// @param priority  Priority level (e.g. "INFO", "WARN").
    /// @param origin  Origin of the message (e.g. application name).
    /// @param time_s  Timestamp in MJD seconds. If 0, uses current time.
    void add_entry(const std::string& message, const std::string& priority = "INFO",
                   const std::string& origin = "casacore_mini", double time_s = 0.0);

    /// Flush history entries to disk.
    void flush();

  private:
    struct HistoryEntry {
        double time = 0.0;
        std::int32_t observation_id = 0;
        std::string message;
        std::string priority;
        std::string origin;
        std::int32_t object_id = 0;
        std::string application;
    };

    MeasurementSet& ms_;
    std::vector<HistoryEntry> entries_;
};

} // namespace casacore_mini
