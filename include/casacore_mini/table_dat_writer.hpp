#pragma once

#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/table_dat.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Write casacore `table.dat` file headers.

/// Write a `table.dat` header into an `AipsIO` writer.
///
/// Writes the Table object header (version 2) with row count, endian flag,
/// and table type string. The output is readable by `parse_table_dat_metadata`.
///
/// @param writer   AipsIO writer to append the header bytes to.
/// @param metadata Metadata to encode (table_version is ignored; always writes v2).
///
/// @throws std::runtime_error if `metadata.row_count` does not fit the v2
/// `uInt` row-count field.
void write_table_dat_header(AipsIoWriter& writer, const TableDatMetadata& metadata);

/// Serialize a `table.dat` header into a standalone byte vector.
///
/// Convenience wrapper around `write_table_dat_header`.
[[nodiscard]] std::vector<std::uint8_t>
serialize_table_dat_header(const TableDatMetadata& metadata);

} // namespace casacore_mini
