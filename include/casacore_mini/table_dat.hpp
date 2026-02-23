#pragma once

#include "casacore_mini/platform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Read top-level metadata from casacore `table.dat` files.

/// Summary metadata parsed from a casacore `table.dat` file header.
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

} // namespace casacore_mini
