// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Flag manipulation for MeasurementSet rows.

/// 
/// Row-level flag manipulation and statistics for a MeasurementSet.
/// 
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS container whose FLAG_ROW column is modified
/// 
///
/// 
/// This header provides three operations on the FLAG_ROW column of the
/// MeasurementSet main table:
///
/// 1. `ms_flag_stats()` — query the total number of rows and the
///    number of rows whose FLAG_ROW is currently set to `true`.
///
/// 2. `ms_flag_rows()` — set FLAG_ROW to `true` for a
///    specified list of row indices.  The main-table SSM data file is
///    rewritten with the updated values.
///
/// 3. `ms_unflag_rows()` — set FLAG_ROW to `false` for a
///    specified list of row indices.
///
/// These functions operate at the row level only.  Per-channel, per-baseline,
/// or per-correlation flag manipulation (i.e. the FLAG array column) is not
/// provided here; use the `Table` write API directly for that.
///
/// Note: because the SSM data file is rewritten on each call, applications
/// that need to apply many individual flag operations should batch the row
/// indices and call these functions once rather than once per row.
/// 
///
/// @par Example
/// Query flagging statistics and then flag a specific subset of rows:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   auto stats = ms_flag_stats(ms);
///   std::cout << stats.flagged_rows << " / " << stats.total_rows
///             << " rows flagged\n";
///
///   // Flag rows 0, 5, and 10 (e.g. shadowed baselines identified earlier)
///   ms_flag_rows(ms, {0, 5, 10});
/// @endcode
/// 
///
/// @par Example
/// Unflag all previously flagged rows:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   std::vector<std::uint64_t> all_rows;
///   all_rows.reserve(ms.row_count());
///   for (std::uint64_t r = 0; r < ms.row_count(); ++r)
///       all_rows.push_back(r);
///   ms_unflag_rows(ms, all_rows);
/// @endcode
/// 

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
