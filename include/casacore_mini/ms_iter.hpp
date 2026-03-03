// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MsIter: iterate over MS rows grouped by (ARRAY_ID, FIELD_ID, DATA_DESC_ID).

///
/// Iteration utilities for grouping MeasurementSet rows by key fields or
/// by time intervals.
///
///
///
///
/// @par Prerequisites
///   - MeasurementSet — the MS container whose main table is scanned
///
///
///
/// This header provides two complementary strategies for iterating over the
/// rows of a MeasurementSet main table:
///
/// 1. Key-field grouping via `ms_iter_chunks`.
///    The main table is scanned once.  Rows that share the same
///    (ARRAY_ID, FIELD_ID, DATA_DESC_ID) triple are collected into an
///    `MsIterChunk`.  The resulting vector of chunks is sorted by
///    (array_id, field_id, data_desc_id, first_row), which matches the
///    canonical casacore MSIter ordering.
///
/// 2. Time-bin grouping via `ms_time_chunks`.
///    The main table is scanned once.  Each row's TIME value is quantised
///    into a bin of width `interval_s` seconds.  Rows falling in the
///    same bin are collected into an `MsTimeChunk` whose
///    `time_center` is the midpoint of the bin.  Within each chunk,
///    rows appear in their original row-index order.
///
/// Both functions return their results in a single pass and do not modify
/// the MeasurementSet.
///
///
/// @par Example
/// Group main-table rows by (ARRAY_ID, FIELD_ID, DATA_DESC_ID) and process
/// each group:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   for (const auto& chunk : ms_iter_chunks(ms)) {
///       std::cout << "field=" << chunk.field_id
///                 << " ddid=" << chunk.data_desc_id
///                 << " rows=" << chunk.rows.size() << "\n";
///   }
/// @endcode
///
///
/// @par Example
/// Group rows into 10-second time bins:
/// @code{.cpp}
///   using namespace casacore_mini;
///   auto ms = MeasurementSet::open("my.ms");
///
///   for (const auto& bin : ms_time_chunks(ms, 10.0)) {
///       std::cout << "t=" << bin.time_center
///                 << " rows=" << bin.rows.size() << "\n";
///   }
/// @endcode
///

/// A chunk of contiguous rows sharing the same grouping key.
struct MsIterChunk {
    std::int32_t array_id = 0;
    std::int32_t field_id = 0;
    std::int32_t data_desc_id = 0;
    /// Row indices in the main table belonging to this chunk.
    std::vector<std::uint64_t> rows;
};

/// Iterate over MeasurementSet rows grouped by (ARRAY_ID, FIELD_ID, DATA_DESC_ID).
///
/// Scans the main table once, collecting rows into chunks. The resulting
/// chunks are sorted by (array_id, field_id, data_desc_id, first_row).
///
/// @param ms  An open MeasurementSet.
/// @return  Vector of iteration chunks.
[[nodiscard]] std::vector<MsIterChunk> ms_iter_chunks(MeasurementSet& ms);

/// Iterate over rows grouped by time intervals.
///
/// Groups rows into bins of `interval_s` seconds. Within each bin, rows are
/// ordered by their original row index.
///
/// @param ms  An open MeasurementSet.
/// @param interval_s  Time bin width in seconds.
/// @return  Vector of (time_center, row_indices) pairs.
struct MsTimeChunk {
    double time_center = 0.0;
    std::vector<std::uint64_t> rows;
};

[[nodiscard]] std::vector<MsTimeChunk> ms_time_chunks(MeasurementSet& ms, double interval_s);

} // namespace casacore_mini
