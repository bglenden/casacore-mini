// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Transitional extraction of measure/coordinate metadata from `showtableinfo` text.

/// 
/// Measure-related metadata for one table column.
/// 
///
///
///
/// 
/// Captures the measure and unit annotations attached to a single column as
/// parsed from `showtableinfo` output.  The parser populates as many fields
/// as are present in the text; absent fields remain as `std::nullopt` or an
/// empty vector.
///
/// This struct is the per-column unit of `MeasureCoordinateMetadata`.
/// 
struct MeasureColumnMetadata {
    /// Column name in table schema.
    std::string column_name;
    /// Quantum/physical units associated with the column.
    std::vector<std::string> quantum_units;
    /// Measure type token (for example `epoch`, `uvw`, `EPOCH`).
    std::optional<std::string> measure_type;
    /// Measure reference token (for example `UTC`, `ITRF`).
    std::optional<std::string> measure_reference;

    [[nodiscard]] bool operator==(const MeasureColumnMetadata& other) const = default;
};

/// 
/// Extracted coordinates-related table keyword metadata.
/// 
///
///
///
/// 
/// Captures the fields from the `coords` top-level keyword sub-record in an
/// image table, which describes the WCS coordinate system stored alongside the
/// image data.  All fields are optional and default to empty/absent; the parser
/// populates only those that appear in the `showtableinfo` text.
///
/// Direction coordinate parameters (`direction0.*`) describe the first
/// directional axis pair.  Map parameters (`worldmap0`, `pixelmap0`, etc.)
/// describe the axis permutation and replacement scheme used by
/// `CoordinateSystem`.
/// 
struct CoordinateKeywordMetadata {
    /// True when a top-level `coords` keyword record is present.
    bool has_coords = false;
    /// `coords.obsdate.type`, when present.
    std::optional<std::string> obsdate_type;
    /// `coords.obsdate.refer`, when present.
    std::optional<std::string> obsdate_reference;
    /// `coords.direction0.system`, when present.
    std::optional<std::string> direction_system;
    /// `coords.direction0.projection`, when present.
    std::optional<std::string> direction_projection;
    /// `coords.direction0.projection_parameters` values.
    std::vector<double> direction_projection_parameters;
    /// `coords.direction0.crval` values.
    std::vector<double> direction_crval;
    /// `coords.direction0.crpix` values.
    std::vector<double> direction_crpix;
    /// `coords.direction0.cdelt` values.
    std::vector<double> direction_cdelt;
    /// `coords.direction0.pc` values in emitted order.
    std::vector<double> direction_pc;
    /// `coords.direction0.axes` entries, when present.
    std::vector<std::string> direction_axes;
    /// `coords.direction0.units` entries, when present.
    std::vector<std::string> direction_units;
    /// `coords.direction0.conversionSystem`, when present.
    std::optional<std::string> direction_conversion_system;
    /// `coords.direction0.longpole`, when present.
    std::optional<double> direction_longpole;
    /// `coords.direction0.latpole`, when present.
    std::optional<double> direction_latpole;
    /// `coords.worldmap0` values.
    std::vector<std::int64_t> worldmap0;
    /// `coords.worldreplace0` values.
    std::vector<double> worldreplace0;
    /// `coords.pixelmap0` values.
    std::vector<std::int64_t> pixelmap0;
    /// `coords.pixelreplace0` values.
    std::vector<double> pixelreplace0;

    [[nodiscard]] bool operator==(const CoordinateKeywordMetadata& other) const = default;
};

/// 
/// Aggregated measure/coordinate metadata extracted from one `showtableinfo` document.
/// 
///
///
///
/// 
/// This is the top-level result type of both `parse_showtableinfo_measure_coordinate_metadata`
/// (text path) and `extract_record_metadata` (binary path).  It collects all
/// column-level measure annotations and the table-level WCS coordinate metadata
/// in a single value that is easy to compare, serialize, and pass around.
/// 
struct MeasureCoordinateMetadata {
    /// Column-level measure metadata entries in first-seen order.
    std::vector<MeasureColumnMetadata> measure_columns;
    /// Table-level coordinate keyword metadata.
    CoordinateKeywordMetadata coordinates;

    [[nodiscard]] bool operator==(const MeasureCoordinateMetadata& other) const = default;
};

/// Parse measure/coordinate metadata from `showtableinfo` textual output.
///
/// 
/// Scope of this transitional parser:
/// - Column keyword forms:
///   - `UNIT: String "..."` (legacy singular unit)
///   - `QuantumUnits: String array ...` with following bracket list line
///   - `MEASURE_TYPE: String "..."`
///   - `MEASURE_REFERENCE: String "..."`
///   - `MEASINFO: { type: String "...", Ref: String "..." }`
/// - Table keyword coordinate forms:
///   - `coords.obsdate.type`
///   - `coords.obsdate.refer`
///   - `coords.direction0.system`
///   - `coords.direction0.projection`
///   - `coords.direction0.projection_parameters`
///   - `coords.direction0.crval`
///   - `coords.direction0.crpix`
///   - `coords.direction0.cdelt`
///   - `coords.direction0.pc`
///   - `coords.direction0.axes`
///   - `coords.direction0.units`
///   - `coords.direction0.conversionSystem`
///   - `coords.direction0.longpole`
///   - `coords.direction0.latpole`
///   - `coords.worldmap0`
///   - `coords.worldreplace0`
///   - `coords.pixelmap0`
///   - `coords.pixelreplace0`
///
/// Missing sections or keyword parse failures produce an empty metadata result.
/// 
[[nodiscard]] MeasureCoordinateMetadata
parse_showtableinfo_measure_coordinate_metadata(std::string_view showtableinfo_text);

} // namespace casacore_mini
