// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

#include <optional>
#include <string>

namespace casacore_mini {

/// @file
/// @brief Observatory/observation metadata (telescope, observer, obsdate).
///
///
///
///
/// ObsInfo bundles the small set of observation-level metadata that
/// casacore-original stores as keywords on a CoordinateSystem.  The same
/// fields are written into the top-level keywords of a MeasurementSet's
/// OBSERVATION subtable by the standard filler tools, making them
/// consistently available for pipeline processing.
///
/// All fields are optional except `telescope` and
/// `observer`, which default to empty strings.  The
/// serialization round-trip via `obs_info_to_record` and
/// `obs_info_from_record` is lossless for all present fields.
///
///
/// @par Example
/// @code{.cpp}
///   // Populate from a CoordinateSystem keyword record
///   Record cs_rec = table.key_record("coords");
///   ObsInfo info = obs_info_from_record(cs_rec);
///
///   std::cout << "Telescope: " << info.telescope << '\n';
///   if (info.obs_date.has_value()) {
///       const auto& ep = std::get<EpochValue>(info.obs_date->value);
///       std::cout << "MJD: " << ep.day + ep.fraction << '\n';
///   }
///
///   // Write back after modification
///   info.observer = "Doe, J.";
///   obs_info_to_record(info, cs_rec);
///   table.put_key_record("coords", cs_rec);
/// @endcode
///
/// @ingroup coordinates
/// @addtogroup coordinates
/// @{

/// Observation metadata typically stored in CoordinateSystem keywords.
///
///
/// <dl>
///   <dt>telescope</dt>
///   <dd>Telescope name string (e.g. "ALMA", "VLA", "MeerKAT").</dd>
///   <dt>observer</dt>
///   <dd>Principal observer name string.</dd>
///   <dt>obs_date</dt>
///   <dd>Observation start epoch as a Measure with EpochRef (usually UTC)
///       and an EpochValue holding the Modified Julian Date split into integer
///       day and fractional remainder for precision.</dd>
///   <dt>telescope_position</dt>
///   <dd>Geocentric ITRF position of the telescope reference point as a
///       Measure with PositionRef::itrf and a PositionValue in metres.</dd>
///   <dt>pointing_center</dt>
///   <dd>Nominal pointing direction in radians as (longitude, latitude)
///       in the direction reference frame of the CoordinateSystem.</dd>
/// </dl>
///
struct ObsInfo {
    std::string telescope;
    std::string observer;
    /// Observation date as an epoch measure (optional).
    std::optional<Measure> obs_date;
    /// Telescope position as a position measure (optional).
    std::optional<Measure> telescope_position;
    /// Pointing center [lon_rad, lat_rad] (optional).
    std::optional<std::pair<double, double>> pointing_center;
};

/// Serialize ObsInfo fields into a Record in casacore CoordinateSystem format.
///
///
/// Writes the following keys into `rec`:
/// <dl>
///   <dt>"telescopeName"</dt>  <dd>string</dd>
///   <dt>"userUnit"</dt>       <dd>string (observer field)</dd>
///   <dt>"obsdate"</dt>        <dd>sub-record from measure_to_record(obs_date)</dd>
///   <dt>"telescopePosition"</dt> <dd>sub-record from measure_to_record(telescope_position)</dd>
///   <dt>"pointingCenter"</dt> <dd>sub-record with "value" array [lon, lat]
///                                 and "initial" bool</dd>
/// </dl>
/// Fields with no value (empty string or absent optional) are not written.
///
///
/// @param info  The ObsInfo to serialize.
/// @param rec   The Record to write into; existing keys are overwritten.
void obs_info_to_record(const ObsInfo& info, Record& rec);

/// Reconstruct ObsInfo from a Record in casacore CoordinateSystem format.
///
///
/// Reads the keys written by `obs_info_to_record`.  Missing keys
/// result in default-initialised fields (empty strings, absent optionals).
/// Unknown key names are silently ignored so that records containing
/// additional metadata remain parseable.
///
///
/// @param rec  The Record to read from.
/// @return Populated ObsInfo; optionals are absent when the key was missing.
[[nodiscard]] ObsInfo obs_info_from_record(const Record& rec);

/// @}
} // namespace casacore_mini
