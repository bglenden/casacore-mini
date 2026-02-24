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
/// Always writes table version 2 format (no per-column keywords in post-TD).
///
/// @throws std::runtime_error if any field exceeds wire format limits.
[[nodiscard]] std::vector<std::uint8_t> serialize_table_dat_full(const TableDatFull& table);

/// Write a complete table.dat body into an AipsIO writer.
///
/// @throws std::runtime_error if any field exceeds wire format limits.
void write_table_dat_full(AipsIoWriter& writer, const TableDatFull& table);

} // namespace casacore_mini
