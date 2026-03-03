// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"

#include <optional>

namespace casacore_mini {

/// @file
/// @brief Measure frame-aware conversions for all 9 measure types.
///
///
///
///
/// Provides `convert_measure`, a single entry point for converting
/// a Measure from one reference frame to another.  Frame-dependent
/// conversions require additional context supplied through a MeasureFrame.
///
/// Supported conversions by measure type:
/// <dl>
///   <dt>MEpoch</dt>
///   <dd>UTC ↔ TAI (leap-second table), TAI ↔ TDT (+32.184 s),
///       TDT ↔ TDB (relativistic), UTC ↔ UT1 (requires dut1 in frame).
///       Identity conversions within the same frame always succeed.</dd>
///   <dt>MPosition</dt>
///   <dd>ITRF ↔ WGS84 using Bowring's method (sub-mm accuracy).</dd>
///   <dt>MDirection</dt>
///   <dd>J2000 ↔ GALACTIC (IAU matrix), J2000 ↔ ECLIPTIC (obliquity),
///       J2000 ↔ ICRS (ICRS offset rotation), J2000 → APP (precession/nutation;
///       requires epoch in frame).  AZEL and HADEC require both position
///       and epoch in the frame.</dd>
///   <dt>MDoppler</dt>
///   <dd>All five conventions (radio, z, ratio, beta, gamma) interconvert
///       via closed-form algebraic relations; no frame context needed.</dd>
///   <dt>MFrequency / MRadialVelocity</dt>
///   <dd>Identity within the same frame.  Cross-frame conversions require
///       observer position and epoch and are not yet implemented; they
///       throw `std::invalid_argument`.</dd>
///   <dt>MBaseline / Muvw</dt>
///   <dd>Frame rotation via the same spherical rotation matrices used for
///       MDirection (J2000/ICRS/GAL/ECL/APP).</dd>
///   <dt>MEarthMagnetic</dt>
///   <dd>Geometric frame rotation for all frames except IGRF, which throws
///       `std::runtime_error` (requires external IGRF coefficient
///       tables not bundled with casacore-mini).</dd>
/// </dl>
///
/// Unsupported frame combinations and conversions requiring missing frame
/// context both throw `std::invalid_argument`.
///
///
/// @par Example
/// @code{.cpp}
///   // Epoch: UTC → TAI
///   Measure utc;
///   utc.type  = MeasureType::epoch;
///   utc.ref   = MeasureRef{EpochRef::utc, std::nullopt};
///   utc.value = EpochValue{59000.0, 0.0};
///   Measure tai = convert_measure(utc, EpochRef::tai);
///   // tai EpochValue.day + fraction == 59000.0 + 37.0/86400.0
///
///   // Direction: J2000 → GALACTIC
///   Measure j2000;
///   j2000.type  = MeasureType::direction;
///   j2000.ref   = MeasureRef{DirectionRef::j2000, std::nullopt};
///   j2000.value = DirectionValue{0.0, 0.0};  // RA=0h, Dec=0deg
///   Measure gal = convert_measure(j2000, DirectionRef::galactic);
///
///   // Topocentric direction needs position+epoch in frame
///   MeasureFrame frame;
///   frame.epoch    = utc;
///   frame.position = vla_position;  // ITRF position of VLA
///   Measure azel = convert_measure(j2000, DirectionRef::azel, frame);
/// @endcode
///
///
/// @par Motivation
/// Centralizing all frame-conversion logic in a single function simplifies
/// the caller API and avoids the complexity of casacore-original's
/// MeasConvert template machinery while covering the conversions actually
/// needed by casacore-mini's coordinate system and imaging modules.
///

/// Context data for measure conversions that require external information.
///
///
/// Some conversions cannot be performed from the measure value alone:
/// <dl>
///   <dt>epoch</dt>
///   <dd>Required for J2000 → APP (precession/nutation), HADEC/AZEL
///       direction conversions, and topocentric frequency shifts.</dd>
///   <dt>position</dt>
///   <dd>Required for topocentric direction (AZEL, HADEC) and topocentric
///       frequency/radial-velocity conversions.</dd>
///   <dt>direction</dt>
///   <dd>Used for UVW rotations when the phase centre must be known.</dd>
///   <dt>dut1</dt>
///   <dd>UT1 − UTC in seconds, required for UTC ↔ UT1 conversions.
///       Typically obtained from IERS bulletins; defaults to 0.0 which
///       introduces an error of at most 0.9 s.</dd>
/// </dl>
/// All fields are optional; a missing field causes the conversion to throw
/// `std::invalid_argument` if the conversion actually requires it.
///
struct MeasureFrame {
    std::optional<Measure> epoch;     ///< Time context (e.g., for precession).
    std::optional<Measure> position;  ///< Location context (e.g., for topocentric).
    std::optional<Measure> direction; ///< Direction context.
    double dut1 = 0.0;                ///< UT1-UTC in seconds (for UTC↔UT1 conversion).
};

/// Convert a Measure to a different reference frame.
///
///
/// Accepts a Measure of any supported type and a target MeasureRefType
/// variant.  The active alternative of `target` must match the
/// measure type of `m`; a mismatch throws
/// `std::invalid_argument`.
///
/// The returned Measure has the same `MeasureType` and
/// `MeasureValue` variant type as `m`, but with values
/// recomputed in the target frame.  Any offset present in `m.ref`
/// is not propagated to the result.
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
///
/// @param m      The input measure.
/// @param target The target reference frame (must match the measure type).
/// @param frame  Context data for frame-dependent conversions.
/// @return A new Measure with the target reference and converted values.
[[nodiscard]] Measure convert_measure(const Measure& m, const MeasureRefType& target,
                                      const MeasureFrame& frame = {});

} // namespace casacore_mini
