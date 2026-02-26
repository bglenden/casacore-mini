#pragma once

#include "casacore_mini/measurement_set.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief MsIter: iterate over MS rows grouped by (ARRAY_ID, FIELD_ID, DATA_DESC_ID).

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
