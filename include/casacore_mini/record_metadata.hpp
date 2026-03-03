// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
/// 
/// This function parallels `parse_showtableinfo_measure_coordinate_metadata`
/// but operates on `Record` objects decoded from AipsIO binary streams rather
/// than parsed text from `showtableinfo` output.
///
/// It traverses the `MEASINFO` and `QuantumUnits` sub-records within column
/// keyword Records, and the `coords` sub-record within the table keyword
/// Record, assembling a `MeasureCoordinateMetadata` result that is identical
/// in structure to the text-parser output.
///
/// Use this path when binary `.bin` fixture files are available alongside a
/// table directory; it is more precise than text parsing and round-trips
/// correctly through the AipsIO codec.
/// 
///
/// @par Example
/// @code{.cpp}
///   AipsIoReader kw_reader(table_keywords_bytes);
///   Record table_kw = read_aipsio_record(kw_reader);
///
///   std::vector<std::pair<std::string, Record>> col_kws;
///   col_kws.emplace_back("TIME", read_aipsio_record(time_reader));
///
///   auto meta = extract_record_metadata(table_kw, col_kws);
/// @endcode
/// 
///
/// @param table_keywords  Top-level table keyword Record.
/// @param column_keywords Column keyword Records as `(column_name, record)` pairs.
/// @return Aggregated measure/coordinate metadata.
[[nodiscard]] MeasureCoordinateMetadata
extract_record_metadata(const Record& table_keywords,
                        const std::vector<std::pair<std::string, Record>>& column_keywords);

} // namespace casacore_mini
