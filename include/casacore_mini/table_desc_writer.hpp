// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/table_desc.hpp"

#include <cstdint>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Serialize full table.dat body (TableDesc + post-TD sections).

/// Serialize a complete table.dat file from a `TableDatFull` structure.
///
/// 
/// Produces the complete binary content of a casacore `table.dat` file,
/// including:
/// - The `Table` object header (version 2 format).
/// - Row count, endian flag, and table type string.
/// - The `TableDesc` object (column descriptors, column keywords, storage
///   manager descriptors, and private table keywords).
/// - Post-TableDesc sections for storage manager setup blobs.
///
/// Always writes table version 2 format.  No per-column keywords are stored
/// in the post-TD section for v2 (they are inlined in the TableDesc).
///
/// `write_table_dat_full` appends into an existing `AipsIoWriter` for
/// callers that compose multiple sections in a single buffer.
/// `serialize_table_dat_full` wraps this in a standalone byte vector for
/// convenience.
/// 
///
/// @par Example
/// @code{.cpp}
///   TableDatFull tdf;
///   tdf.header.row_count = 500;
///   tdf.header.big_endian = false;
///   tdf.header.table_type = "PlainTable";
///   // populate tdf.desc with column descriptors ...
///   auto bytes = serialize_table_dat_full(tdf);
///   std::ofstream("my_table/table.dat", std::ios::binary).write(
///       reinterpret_cast<const char*>(bytes.data()), bytes.size());
/// @endcode
/// 
///
/// @throws std::runtime_error if any field exceeds wire format limits.
[[nodiscard]] std::vector<std::uint8_t> serialize_table_dat_full(const TableDatFull& table);

/// Write a complete table.dat body into an AipsIO writer.
///
/// @throws std::runtime_error if any field exceeds wire format limits.
void write_table_dat_full(AipsIoWriter& writer, const TableDatFull& table);

} // namespace casacore_mini
