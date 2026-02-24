#pragma once

#include "casacore_mini/measure_coord_metadata.hpp"

#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Read measure/coordinate metadata from binary table artifacts.

/// Read measure/coordinate metadata from a table directory's binary files.
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
/// @param fixture_dir Path to the corpus fixture directory.
/// @return Aggregated measure/coordinate metadata from binary sources.
///
/// @throws std::runtime_error if a `.bin` file is present but malformed.
[[nodiscard]] MeasureCoordinateMetadata read_table_binary_metadata(std::string_view fixture_dir);

} // namespace casacore_mini
