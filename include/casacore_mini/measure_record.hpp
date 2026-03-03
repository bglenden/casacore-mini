// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Measure serialization to/from Record in MeasureHolder format.
///
///
///
///
/// Provides lossless round-trip serialization of Measure values to and from
/// casacore-original's MeasureHolder Record layout, enabling storage as
/// table keywords, column keywords, and CoordinateSystem sub-records.
///
/// The on-disk Record layout is:
/// <dl>
///   <dt>"type"</dt>   <dd>string — lowercase measure type name (e.g. "epoch").</dd>
///   <dt>"refer"</dt>  <dd>string — reference frame name (e.g. "UTC", "J2000").</dd>
///   <dt>"m0"</dt>     <dd>sub-Record { "value": double, "unit": string } — first component.</dd>
///   <dt>"m1"</dt>     <dd>sub-Record (optional) — second component (direction, position, …).</dd>
///   <dt>"m2"</dt>     <dd>sub-Record (optional) — third component (3-vector measures).</dd>
///   <dt>"offset"</dt> <dd>sub-Record (optional) — recursive MeasureHolder for the offset.</dd>
/// </dl>
///
/// Component count by measure type:
/// <dl>
///   <dt>epoch</dt>          <dd>2 components (day, fraction), unit "d".</dd>
///   <dt>direction</dt>      <dd>2 components (lon, lat), unit "rad".</dd>
///   <dt>position</dt>       <dd>3 components (x, y, z), unit "m".</dd>
///   <dt>frequency</dt>      <dd>1 component (Hz), unit "Hz".</dd>
///   <dt>doppler</dt>        <dd>1 component (ratio), unit "1".</dd>
///   <dt>radial_velocity</dt><dd>1 component (m/s), unit "m/s".</dd>
///   <dt>baseline</dt>       <dd>3 components (x, y, z), unit "m".</dd>
///   <dt>uvw</dt>            <dd>3 components (u, v, w), unit "m".</dd>
///   <dt>earth_magnetic</dt> <dd>3 components (x, y, z), unit "T".</dd>
/// </dl>
///
///
/// @par Example
/// @code{.cpp}
///   // Serialize an epoch to a Record
///   Measure ep;
///   ep.type  = MeasureType::epoch;
///   ep.ref   = MeasureRef{EpochRef::utc, std::nullopt};
///   ep.value = EpochValue{59000.0, 0.5};
///   Record rec = measure_to_record(ep);
///   // rec["type"] == "epoch", rec["refer"] == "UTC"
///   // rec["m0"]["value"] == 59000.0, rec["m0"]["unit"] == "d"
///   // rec["m1"]["value"] == 0.5,     rec["m1"]["unit"] == "d"
///
///   // Round-trip
///   Measure ep2 = measure_from_record(rec);
///   assert(ep == ep2);
///
///   // Read from an existing table keyword
///   Record obs_rec = table.key_record("EPOCH");
///   Measure obs_ep = measure_from_record(obs_rec);
/// @endcode
///
///
/// @par Motivation
/// casacore-original stores measures as MeasureHolder Records with a
/// well-defined layout used across all table metadata (SPECTRAL_WINDOW,
/// SOURCE, FIELD, etc.).  Implementing the same layout allows casacore-mini
/// tables to be read by upstream casacore tools and vice versa without a
/// conversion step.
///
/// @ingroup measures
/// @addtogroup measures
/// @{

/// Convert a Measure to a Record matching casacore's MeasureHolder format.
///
///
/// Serializes all fields of the Measure including the optional offset.
/// The returned Record is a self-contained representation that can be
/// stored as a table keyword or CoordinateSystem sub-record and later
/// reconstructed with `measure_from_record`.
///
///
/// @param m  The measure to serialize.
/// @return   A Record in MeasureHolder format.
///
/// Record layout:
/// - `type`: string (lowercase measure type name)
/// - `refer`: string (reference frame name)
/// - `m0`: sub-record `{value: double, unit: string}` (Quantity)
/// - `m1`: optional sub-record (2+ component measures)
/// - `m2`: optional sub-record (3-component measures)
/// - `offset`: optional sub-record (recursive Measure for offset)
[[nodiscard]] Record measure_to_record(const Measure& m);

/// Parse a Record in MeasureHolder format back to a Measure.
///
///
/// Reads all fields written by `measure_to_record` and
/// reconstructs the Measure value, including any nested offset.
/// Field names are matched case-insensitively for the "type" and "refer"
/// keys to accommodate minor formatting differences in older data files.
///
///
/// @param rec  The Record to parse.
/// @return     The reconstructed Measure.
///
/// @throws std::invalid_argument on missing/invalid fields, unknown type/refer,
///         wrong value count, or corrupt nested offset.
[[nodiscard]] Measure measure_from_record(const Record& rec);

/// @}
} // namespace casacore_mini
