#pragma once

#include "casacore_mini/measure_types.hpp"

#include <optional>

namespace casacore_mini {

/// @file
/// @brief Measure frame-aware conversions for all 9 measure types.

/// Context data for measure conversions that need external information.
///
/// Some conversions require additional context:
/// - J2000→APP needs an epoch (for precession/nutation)
/// - HADEC needs both position and epoch
/// - UTC→UT1 needs dut1 (UT1-UTC in seconds)
struct MeasureFrame {
    std::optional<Measure> epoch;     ///< Time context (e.g., for precession).
    std::optional<Measure> position;  ///< Location context (e.g., for topocentric).
    std::optional<Measure> direction; ///< Direction context.
    double dut1 = 0.0;                ///< UT1-UTC in seconds (for UTC↔UT1 conversion).
};

/// Convert a Measure to a different reference frame.
///
/// Supports all 9 measure types:
/// - MEpoch: UTC↔TAI, TAI↔TDT(TT), TDT↔TDB, UTC↔UT1 (requires dut1 in frame)
/// - MPosition: ITRF↔WGS84
/// - MDirection: J2000↔GALACTIC, J2000↔ECLIPTIC, J2000↔ICRS, J2000→APP
/// - MDoppler: radio↔z↔beta↔gamma↔ratio (purely mathematical)
/// - MFrequency: same-frame identity; cross-frame throws (requires observer context)
/// - MRadialVelocity: same-frame identity; cross-frame throws (requires observer context)
/// - MBaseline: frame rotation via spherical direction conversion (J2000/ICRS/GAL/ECL/APP)
/// - Muvw: same pattern as MBaseline
/// - MEarthMagnetic: frame rotation; IGRF paths throw std::runtime_error
///
/// Unsupported frame combinations throw `std::invalid_argument`.
/// Conversions requiring missing frame data throw `std::invalid_argument`.
///
/// @param m      The input measure.
/// @param target The target reference frame (must match the measure type).
/// @param frame  Context data for frame-dependent conversions.
/// @return A new Measure with the target reference and converted values.
[[nodiscard]] Measure convert_measure(const Measure& m, const MeasureRefType& target,
                                      const MeasureFrame& frame = {});

} // namespace casacore_mini
