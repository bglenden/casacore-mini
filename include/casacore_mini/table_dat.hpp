#pragma once

#include "casacore_mini/platform.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace casacore_mini {

/// Summary metadata parsed from a casacore `table.dat` file header.
struct TableDatMetadata {
    std::uint32_t table_version = 0;
    std::uint64_t row_count = 0;
    bool big_endian = true;
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
[[nodiscard]] TableDatMetadata parse_table_dat_metadata(std::span<const std::uint8_t> bytes);

/// Read and parse top-level metadata from a `table.dat` file path.
[[nodiscard]] TableDatMetadata read_table_dat_metadata(std::string_view table_dat_path);

} // namespace casacore_mini
