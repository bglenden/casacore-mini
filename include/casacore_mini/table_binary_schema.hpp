// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_coord_metadata.hpp"

#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Read measure/coordinate metadata from binary table artifacts.
/// @ingroup tables
/// @addtogroup tables
/// @{

/// Read measure/coordinate metadata from a table directory's binary files.
///
///
/// This function reads `table.dat` (header only) plus any keyword `.bin`
/// files found alongside `showtableinfo.txt` in the fixture directory,
/// decodes them via `read_aipsio_record`, and extracts metadata via
/// `extract_record_metadata`.
///
/// The fixture directory is expected to contain:
/// - `table_keywords.bin` (optional; top-level table keyword Record)
/// - `column_<NAME>_keywords.bin` (optional; per-column keyword Records)
///
/// Files that are absent are silently skipped.  Files that are present but
/// malformed cause an exception.  This function is the binary counterpart to
/// `parse_showtableinfo_measure_coordinate_metadata`, providing higher-fidelity
/// metadata extraction without depending on text formatting.
///
///
/// @par Example
/// @code{.cpp}
///   auto meta = read_table_binary_metadata("corpus/ms_fixture");
///   for (const auto& col : meta.measure_columns) {
///       std::cout << col.column_name << " " << col.measure_type.value_or("?") << "\n";
///   }
/// @endcode
///
///
/// @param fixture_dir Path to the corpus fixture directory.
/// @return Aggregated measure/coordinate metadata from binary sources.
///
/// @throws std::runtime_error if a `.bin` file is present but malformed.
[[nodiscard]] MeasureCoordinateMetadata read_table_binary_metadata(std::string_view fixture_dir);

/// @}
} // namespace casacore_mini
