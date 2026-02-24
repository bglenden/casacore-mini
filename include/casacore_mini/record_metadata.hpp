#pragma once

#include "casacore_mini/measure_coord_metadata.hpp"
#include "casacore_mini/record.hpp"

#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Extract measure/coordinate metadata from binary-decoded `Record` objects.

/// Extract measure/coordinate metadata from binary-decoded Records.
///
/// This parallels `parse_showtableinfo_measure_coordinate_metadata` but operates
/// on `Record` objects decoded from AipsIO binary streams rather than parsed text.
///
/// @param table_keywords  Top-level table keyword Record.
/// @param column_keywords Column keyword Records as `(column_name, record)` pairs.
/// @return Aggregated measure/coordinate metadata.
[[nodiscard]] MeasureCoordinateMetadata
extract_record_metadata(const Record& table_keywords,
                        const std::vector<std::pair<std::string, Record>>& column_keywords);

} // namespace casacore_mini
