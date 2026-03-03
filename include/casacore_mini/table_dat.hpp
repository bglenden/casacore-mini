// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Read top-level metadata from casacore `table.dat` files.
/// @ingroup tables
/// @addtogroup tables
/// @{

///
/// Summary metadata parsed from a casacore `table.dat` file header.
///
///
///
///
///
/// A casacore table directory always contains a `table.dat` file whose first
/// `AipsIO` object is typed `"Table"`.  The `TableDatMetadata` struct captures
/// the four fields that appear immediately after the object header:
///
/// - `table_version` — the `AipsIO` object version, which governs how the
///   row count is encoded (32-bit for v1, 64-bit for v2+).
/// - `row_count` — number of rows in the table.
/// - `big_endian` — endianness flag stored in the file; `true` when the flag
///   byte is `0` (big-endian), `false` when it is `1` (little-endian).
/// - `table_type` — the concrete table implementation token written next,
///   typically `"PlainTable"`.
///
/// This structure is populated by `parse_table_dat_metadata` and
/// `read_table_dat_metadata`, and consumed by the storage-manager readers
/// to select the correct byte-swap path.
///
struct TableDatMetadata {
    /// `Table` object version from the root `AipsIO` object header.
    std::uint32_t table_version = 0;
    /// Row count from top-level table metadata (`u32` for older versions, `u64` otherwise).
    std::uint64_t row_count = 0;
    /// Persisted table-endian flag (`true` when flag is `0`).
    bool big_endian = true;
    /// Table implementation token, for example `PlainTable`.
    std::string table_type;

    [[nodiscard]] bool operator==(const TableDatMetadata& other) const = default;
};

/// Parse top-level table metadata from `table.dat` bytes.
///
/// This reads:
/// - `Table` object header (`AipsIO`)
/// - row count
/// - table data endianness flag (`0=big`, `1=little`)
/// - table implementation type string (for example `PlainTable`)
///
/// @throws std::runtime_error if root object is not `Table`, endian flag is not
/// `0/1`, or input is truncated/malformed.
[[nodiscard]] TableDatMetadata parse_table_dat_metadata(std::span<const std::uint8_t> bytes);

/// Read and parse top-level metadata from a `table.dat` file path.
///
/// @throws std::runtime_error if file cannot be opened or payload is malformed.
[[nodiscard]] TableDatMetadata read_table_dat_metadata(std::string_view table_dat_path);

/// @}
} // namespace casacore_mini
