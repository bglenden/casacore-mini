#pragma once

#include "casacore_mini/record.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Types for full table.dat body: TableDesc, column descriptors,
/// storage-manager metadata.

/// casacore DataType enum values used in column descriptors.
// NOLINTNEXTLINE(performance-enum-size)
enum class DataType : std::int32_t {
    tp_bool = 0,
    tp_char = 1,
    tp_uchar = 2,
    tp_short = 3,
    tp_ushort = 4,
    tp_int = 5,
    tp_uint = 6,
    tp_float = 7,
    tp_double = 8,
    tp_complex = 9,
    tp_dcomplex = 10,
    tp_string = 11,
    tp_table = 12,
    tp_int64 = 29,
};

/// Column descriptor kind (scalar vs array).
enum class ColumnKind : std::uint8_t {
    scalar = 0,
    array = 1,
};

/// Parsed column descriptor from a `table.dat` `TableDesc` section.
struct ColumnDesc {
    /// Column kind (scalar or array).
    ColumnKind kind = ColumnKind::scalar;
    /// AipsIO type string (e.g. `ScalarColumnDesc<Int     >`).
    std::string type_string{};
    /// AipsIO object version for the column descriptor.
    std::uint32_t version = 0;
    /// Column name.
    std::string name{};
    /// Column comment.
    std::string comment{};
    /// Data manager type (e.g. `StManAipsIO`).
    std::string dm_type{};
    /// Data manager group name.
    std::string dm_group{};
    /// casacore DataType code.
    DataType data_type = DataType::tp_int;
    /// Column options bitmask.
    std::int32_t options = 0;
    /// Number of dimensions (-1 = any, 0 = scalar, >0 = fixed).
    std::int32_t ndim = 0;
    /// Fixed shape (IPosition) when ndim > 0.
    std::vector<std::int64_t> shape{};
    /// Maximum string length (0 = unlimited).
    std::int32_t max_length = 0;
    /// Column keywords (TableRecord).
    Record keywords{};

    [[nodiscard]] bool operator==(const ColumnDesc& other) const = default;
};

/// Storage manager descriptor entry from the post-TableDesc section.
struct StorageManagerSetup {
    /// Storage manager type name (e.g. `StManAipsIO`).
    std::string type_name{};
    /// Sequence number.
    std::uint32_t sequence_number = 0;
    /// Raw data blob for this SM (from AipsIO getnew in table.dat).
    /// Parsed by the individual SM reader (e.g. SSM reader).
    std::vector<std::uint8_t> data_blob{};

    [[nodiscard]] bool operator==(const StorageManagerSetup& other) const = default;
};

/// Per-column storage manager assignment from the post-TableDesc section.
struct ColumnManagerSetup {
    /// Column name.
    std::string column_name{};
    /// SM sequence number this column belongs to.
    std::uint32_t sequence_number = 0;
    /// True if column has a fixed shape (array columns only).
    bool has_shape = false;
    /// Fixed shape (only meaningful when has_shape is true).
    std::vector<std::int64_t> shape{};

    [[nodiscard]] bool operator==(const ColumnManagerSetup& other) const = default;
};

/// Full parsed TableDesc from a table.dat body.
struct TableDesc {
    /// TableDesc AipsIO version.
    std::uint32_t version = 0;
    /// Table name.
    std::string name{};
    /// Table comment/description.
    std::string comment{};
    /// Table-level keywords (from TableDesc section).
    Record keywords{};
    /// Table-level private keywords.
    Record private_keywords{};
    /// Column descriptors in file order.
    std::vector<ColumnDesc> columns{};

    [[nodiscard]] bool operator==(const TableDesc& other) const = default;
};

/// Full parsed table.dat contents (header + body).
struct TableDatFull {
    /// Table object version.
    std::uint32_t table_version = 0;
    /// Row count.
    std::uint64_t row_count = 0;
    /// Big-endian flag.
    bool big_endian = true;
    /// Table type string (e.g. `PlainTable`).
    std::string table_type{};
    /// Table descriptor.
    TableDesc table_desc;
    /// Table keywords from post-TD section (tableVer=1 only; empty for v2+).
    std::optional<Record> table_keywords_v1{};
    /// Repeated row count from post-TD section.
    std::uint64_t post_td_row_count = 0;
    /// Storage manager descriptors.
    std::vector<StorageManagerSetup> storage_managers{};
    /// Per-column storage manager assignments.
    std::vector<ColumnManagerSetup> column_setups{};

    [[nodiscard]] bool operator==(const TableDatFull& other) const = default;
};

/// Parse full table.dat contents from bytes.
///
/// @throws std::runtime_error on malformed input.
[[nodiscard]] TableDatFull parse_table_dat_full(std::span<const std::uint8_t> bytes);

/// Read and parse full table.dat from a file path.
///
/// @throws std::runtime_error if file cannot be opened or payload is malformed.
[[nodiscard]] TableDatFull read_table_dat_full(std::string_view table_dat_path);

} // namespace casacore_mini
