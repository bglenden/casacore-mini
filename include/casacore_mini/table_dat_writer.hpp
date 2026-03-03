// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
/// <synopsis>
/// Serializes the `Table` object header (always at version 2 regardless of
/// the `table_version` field in `metadata`) followed by:
/// - row count as a 32-bit `uInt` (version 2 wire format)
/// - endian flag byte (`0` for big-endian, `1` for little-endian)
/// - table type string (e.g. `"PlainTable"`)
///
/// The output is readable by `parse_table_dat_metadata` and compatible with
/// casacore's `Table::open` reader path.
///
/// `serialize_table_dat_header` is a convenience wrapper that internally
/// constructs an `AipsIoWriter`, calls `write_table_dat_header`, and returns
/// the accumulated bytes as a standalone vector.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   TableDatMetadata meta;
///   meta.row_count = 1000;
///   meta.big_endian = false;
///   meta.table_type = "PlainTable";
///   auto bytes = serialize_table_dat_header(meta);
/// </srcblock>
/// </example>
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
