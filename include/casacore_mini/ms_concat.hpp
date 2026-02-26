#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace casacore_mini {

/// @file
/// @brief Concatenate two MeasurementSets into one.

/// Result of an MS concatenation.
struct MsConcatResult {
    /// Number of rows in the output MS.
    std::uint64_t row_count = 0;
    /// Per-subtable: old→new ID mapping for the second MS.
    std::map<std::string, std::map<std::int32_t, std::int32_t>> id_remaps;
};

/// Concatenate two MeasurementSets into a new output MS.
///
/// The output MS contains all rows from both inputs. Subtable rows from
/// ms2 are appended to ms1's subtables, with ID columns in the main table
/// remapped accordingly. Duplicate subtable rows (same NAME for ANTENNA,
/// same NAME for FIELD, etc.) are deduplicated by name.
///
/// @param ms1  First (base) MeasurementSet.
/// @param ms2  Second MeasurementSet to append.
/// @param output_path  Path for the new concatenated MS.
/// @return  Concat result with row count and ID remapping info.
/// @throws std::runtime_error if schemas are incompatible.
[[nodiscard]] MsConcatResult ms_concat(MeasurementSet& ms1, MeasurementSet& ms2,
                                       const std::filesystem::path& output_path);

} // namespace casacore_mini
