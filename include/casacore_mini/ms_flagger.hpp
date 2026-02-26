#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Flag manipulation for MeasurementSet rows.

/// Flag statistics for a MeasurementSet.
struct MsFlagStats {
    std::uint64_t total_rows = 0;
    std::uint64_t flagged_rows = 0;
};

/// Query FLAG_ROW statistics for a MeasurementSet.
///
/// @param ms  The MeasurementSet to query.
/// @return  Flag statistics.
[[nodiscard]] MsFlagStats ms_flag_stats(MeasurementSet& ms);

/// Set FLAG_ROW for specific rows.
///
/// Rewrites the main-table SSM data file with updated FLAG_ROW values.
///
/// @param ms  The MeasurementSet to modify.
/// @param rows  Row indices to flag (set FLAG_ROW=true).
void ms_flag_rows(MeasurementSet& ms, const std::vector<std::uint64_t>& rows);

/// Clear FLAG_ROW for specific rows.
///
/// @param ms  The MeasurementSet to modify.
/// @param rows  Row indices to unflag (set FLAG_ROW=false).
void ms_unflag_rows(MeasurementSet& ms, const std::vector<std::uint64_t>& rows);

} // namespace casacore_mini
