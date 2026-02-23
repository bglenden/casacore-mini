#pragma once

#include "casacore_mini/platform.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Transitional extraction of measure/coordinate metadata from `showtableinfo` text.

/// Measure-related metadata for one table column.
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

/// Extracted coordinates-related table keyword metadata.
struct CoordinateKeywordMetadata {
    /// True when a top-level `coords` keyword record is present.
    bool has_coords = false;
    /// `coords.obsdate.type`, when present.
    std::optional<std::string> obsdate_type;
    /// `coords.obsdate.refer`, when present.
    std::optional<std::string> obsdate_reference;
    /// `coords.direction0.system`, when present.
    std::optional<std::string> direction_system;
    /// `coords.direction0.axes` entries, when present.
    std::vector<std::string> direction_axes;

    [[nodiscard]] bool operator==(const CoordinateKeywordMetadata& other) const = default;
};

/// Aggregated measure/coordinate metadata extracted from one `showtableinfo` document.
struct MeasureCoordinateMetadata {
    /// Column-level measure metadata entries in first-seen order.
    std::vector<MeasureColumnMetadata> measure_columns;
    /// Table-level coordinate keyword metadata.
    CoordinateKeywordMetadata coordinates;

    [[nodiscard]] bool operator==(const MeasureCoordinateMetadata& other) const = default;
};

/// Parse measure/coordinate metadata from `showtableinfo` textual output.
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
///   - `coords.direction0.axes`
///
/// Missing sections produce an empty metadata result; malformed optional
/// fragments are ignored rather than throwing.
[[nodiscard]] MeasureCoordinateMetadata
parse_showtableinfo_measure_coordinate_metadata(std::string_view showtableinfo_text);

} // namespace casacore_mini
