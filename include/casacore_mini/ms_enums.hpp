// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table_desc.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Main-table column/keyword enums and metadata for MeasurementSet.

/// 
/// Strongly-typed enumerations and metadata descriptors for MeasurementSet
/// main-table columns and keyword references.
/// 
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the high-level MS container
///   - TableDesc — column descriptor types (DataType, ColumnKind)
/// 
///
/// 
/// This header defines three tightly related things:
///
/// 1. `MsMainColumn` — a typed enum over all 21 required and 8
///    optional columns in the MS main table.  The first 21 enumerators
///    (antenna1 through weight) correspond to the MS2 mandatory columns.
///    The optional columns (data through flag_spectrum) follow.
///
/// 2. `MsMainKeyword` — a typed enum over the 17 subtable keyword
///    references stored in the main table's keyword set.  The 12 required
///    subtable keywords and 5 optional subtable keywords are all listed.
///
/// 3. `MsColumnInfo` — a plain-old-data struct carrying the wire
///    representation of one column: its name, data type, kind (scalar or
///    array), number of dimensions, fixed shape (if any), comment, and
///    whether it is required by the MS2 standard.
///
/// The companion free functions provide the mapping layer between enum
/// values, string names, and `MsColumnInfo` structs that the rest
/// of the codebase uses to build column descriptors and validate data.
/// 
///
/// @par Example
/// Look up metadata for a column by enum value:
/// @code{.cpp}
///   using namespace casacore_mini;
///   const auto& info = ms_main_column_info(MsMainColumn::time);
///   // info.name == "TIME", info.data_type == DataType::tp_double, etc.
/// @endcode
/// 
///
/// @par Example
/// Resolve a column name string to its enum and check whether it is required:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto col = ms_main_column_from_name("CORRECTED_DATA");
///   if (col.has_value()) {
///       const auto& info = ms_main_column_info(*col);
///       bool req = info.required; // false for optional columns
///   }
/// @endcode
/// 
///
/// @par Example
/// Iterate over all required subtable names:
/// @code{.cpp}
///   for (const auto& name : ms_required_subtable_names())
///       std::cout << name << "\n";
///   // prints ANTENNA, DATA_DESCRIPTION, FEED, FIELD, ...
/// @endcode
/// 

/// Column enums for the MS main table.
/// Required columns (MS2 standard) come first; optional columns follow.
// NOLINTNEXTLINE(performance-enum-size)
enum class MsMainColumn : std::int32_t {
    // Required columns (21):
    antenna1,
    antenna2,
    array_id,
    data_desc_id,
    exposure,
    feed1,
    feed2,
    field_id,
    flag,
    flag_category,
    flag_row,
    interval,
    observation_id,
    processor_id,
    scan_number,
    sigma,
    state_id,
    time,
    time_centroid,
    uvw,
    weight,
    // Optional columns:
    data,
    float_data,
    corrected_data,
    model_data,
    imaging_weight,
    weight_spectrum,
    sigma_spectrum,
    flag_spectrum,
};

/// Keyword names for the MS main table (subtable references).
// NOLINTNEXTLINE(performance-enum-size)
enum class MsMainKeyword : std::int32_t {
    antenna,
    data_description,
    doppler,
    feed,
    field,
    flag_cmd,
    freq_offset,
    history,
    observation,
    pointing,
    polarization,
    processor,
    source,
    spectral_window,
    state,
    syscal,
    weather,
};

/// Metadata for a single MS column.
struct MsColumnInfo {
    /// Column name as it appears in the table (e.g. "ANTENNA1").
    std::string name;
    /// casacore DataType code.
    DataType data_type = DataType::tp_int;
    /// Scalar or array.
    ColumnKind kind = ColumnKind::scalar;
    /// Number of dimensions (0 for scalar, >0 for array).
    std::int32_t ndim = 0;
    /// Fixed shape (empty for scalars or variable-shape arrays).
    std::vector<std::int64_t> shape;
    /// Column comment/description.
    std::string comment;
    /// Whether this column is required by the MS2 standard.
    bool required = true;
};

/// Look up metadata for a main-table column enum value.
[[nodiscard]] const MsColumnInfo& ms_main_column_info(MsMainColumn col);

/// Resolve a column name string to a main-table column enum.
/// Returns std::nullopt if the name is not recognized.
[[nodiscard]] std::optional<MsMainColumn> ms_main_column_from_name(std::string_view name);

/// Get the table keyword name for a main-table keyword enum value.
[[nodiscard]] std::string_view ms_main_keyword_name(MsMainKeyword kw);

/// Check whether a keyword corresponds to a required subtable.
[[nodiscard]] bool ms_main_keyword_required(MsMainKeyword kw);

/// Get all required main-table column info entries.
[[nodiscard]] std::vector<MsColumnInfo> ms_required_main_columns();

/// Names of the 12 required subtables.
[[nodiscard]] std::vector<std::string> ms_required_subtable_names();

/// Names of all 17 subtables (required + optional).
[[nodiscard]] std::vector<std::string> ms_all_subtable_names();

} // namespace casacore_mini
