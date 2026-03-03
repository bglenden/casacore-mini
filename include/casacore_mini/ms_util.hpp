// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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

/// <summary>
/// Converts complex visibility data between Stokes/correlation bases.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> MeasurementSet — source of the correlation type metadata (POLARIZATION subtable)
/// </prerequisite>
///
/// <synopsis>
/// <src>StokesConverter</src> transforms a vector of complex visibilities
/// from one set of correlation products to another.  Stokes codes follow the
/// FITS/AIPS convention:
///
/// <ul>
///   <li> I=1, Q=2, U=3, V=4
///   <li> RR=5, RL=6, LR=7, LL=8
///   <li> XX=9, XY=10, YX=11, YY=12
/// </ul>
///
/// The converter is constructed once with the input and output type lists.
/// Subsequent calls to <src>convert()</src> apply the same linear
/// transformation to each visibility vector.  For circular feed data the
/// standard relations are:
///
/// <ul>
///   <li> I = (RR + LL) / 2
///   <li> Q = (RL + LR) / 2
///   <li> U = i*(LR - RL) / 2
///   <li> V = (RR - LL) / 2
/// </ul>
/// </synopsis>
///
/// <example>
/// Convert RR, RL, LR, LL visibilities to Stokes I, Q, U, V:
/// <srcblock>
///   using namespace casacore_mini;
///   StokesConverter conv({5,6,7,8}, {1,2,3,4}); // RRLLLR→IQUV
///
///   std::vector<std::complex<float>> in_vis = /* ... */;
///   auto out_vis = conv.convert(in_vis); // length 4
/// </srcblock>
/// </example>
class StokesConverter {
  public:
    /// Set up conversion from input to output Stokes types.
    /// @param in_types  Input correlation types (e.g. {5,6,7,8} for RR,RL,LR,LL).
    /// @param out_types  Desired output types (e.g. {1,2,3,4} for I,Q,U,V).
    StokesConverter(std::vector<std::int32_t> in_types, std::vector<std::int32_t> out_types);

    /// Convert a visibility vector from input to output Stokes basis.
    ///
    /// @param in_data  Complex visibilities in input basis [n_in].
    /// @return  Visibilities in output basis [n_out].
    [[nodiscard]] std::vector<std::complex<float>>
    convert(const std::vector<std::complex<float>>& in_data) const;

    /// Number of input correlations.
    [[nodiscard]] std::size_t n_in() const noexcept {
        return in_types_.size();
    }
    /// Number of output correlations.
    [[nodiscard]] std::size_t n_out() const noexcept {
        return out_types_.size();
    }

  private:
    std::vector<std::int32_t> in_types_;
    std::vector<std::int32_t> out_types_;
};

// ---------------------------------------------------------------------------
// MsDopplerUtil
// ---------------------------------------------------------------------------

/// <summary>
/// Stateless Doppler conversion utilities for MS frequency/velocity data.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// <src>MsDopplerUtil</src> provides radio-convention Doppler conversions
/// between observed frequency and line-of-sight velocity, given the rest
/// frequency of a spectral line.
///
/// The radio velocity convention is used:
/// <ul>
///   <li> v = c * (f_rest - f_obs) / f_rest
///   <li> f_obs = f_rest * (1 - v/c)
/// </ul>
///
/// Rest frequencies are typically found in the SOURCE subtable
/// (REST_FREQUENCY column).  These utilities are designed to work alongside
/// the DOPPLER and FREQ_OFFSET optional subtables.
/// </synopsis>
///
/// <example>
/// Convert a 21 cm HI rest frequency line to velocity at a given observed
/// frequency:
/// <srcblock>
///   using namespace casacore_mini;
///   double rest  = 1.420405751e9; // Hz
///   double f_obs = 1.418e9;       // Hz
///   double v = MsDopplerUtil::frequency_to_velocity(f_obs, rest);
///   // v ≈ +515 km/s (recession)
/// </srcblock>
/// </example>

/// Doppler tracking utility for MS data.
///
/// Wraps VelocityMachine conversions in an MS context, providing
/// velocity-to-frequency and frequency-to-velocity conversions
/// using the rest frequency from the SOURCE subtable.
struct MsDopplerUtil {
    /// Convert radio velocity (m/s) to frequency (Hz) given rest frequency.
    [[nodiscard]] static double velocity_to_frequency(double velocity_mps, double rest_freq_hz);

    /// Convert frequency (Hz) to radio velocity (m/s) given rest frequency.
    [[nodiscard]] static double frequency_to_velocity(double freq_hz, double rest_freq_hz);
};

// ---------------------------------------------------------------------------
// MsHistoryHandler
// ---------------------------------------------------------------------------

/// <summary>
/// Appends processing history entries to the HISTORY subtable of a
/// MeasurementSet.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> MeasurementSet — the MS whose HISTORY subtable is written
/// </prerequisite>
///
/// <synopsis>
/// <src>MsHistoryHandler</src> buffers one or more history log entries in
/// memory and writes them to the HISTORY subtable when <src>flush()</src> is
/// called.  Each entry carries a timestamp (MJD seconds), an observation ID,
/// a free-text message, a priority level string, and an origin identifier.
///
/// If the <src>time_s</src> argument to <src>add_entry()</src> is zero, the
/// handler substitutes the current wall-clock time expressed in MJD seconds.
///
/// Priority strings follow the casacore convention: "DEBUGGING", "INFO",
/// "WARN", "SEVERE".
/// </synopsis>
///
/// <example>
/// Log two processing steps and flush to disk:
/// <srcblock>
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///   MsHistoryHandler hist(ms);
///
///   hist.add_entry("Flagged shadowed antennas", "INFO", "flagger");
///   hist.add_entry("Calibration applied", "INFO", "applycal");
///   hist.flush();
/// </srcblock>
/// </example>

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
