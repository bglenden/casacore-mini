// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
/// @ingroup tables
/// @addtogroup tables
/// @{

///
/// casacore DataType enum values used in column descriptors.
///
///
///
///
///
/// DataType maps directly to the integer codes stored in casacore AipsIO
/// column descriptors inside `table.dat`.  Each enumerator
/// corresponds to one of the fundamental casacore numeric or string types.
///
/// The underlying representation is `int32_t` to match the
/// wire format; the enumerator values are therefore fixed and must not
/// be renumbered.
///
/// Common values:
///
///   - `tp_bool`    (0)  — boolean
///   - `tp_int`     (5)  — 32-bit signed integer
///   - `tp_float`   (7)  — 32-bit IEEE float
///   - `tp_double`  (8)  — 64-bit IEEE double
///   - `tp_complex` (9)  — complex<float>
///   - `tp_dcomplex`(10) — complex<double>
///   - `tp_string`  (11) — UTF-8 string
///   - `tp_table`   (12) — sub-table reference
///   - `tp_int64`   (29) — 64-bit signed integer
///
///
/// Use `parse_data_type_name()` to map TaQL/TableDesc name strings
/// to this enum, and `data_type_to_string()` for the reverse.
///
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

///
/// Column descriptor kind distinguishing scalar from array columns.
///
///
///
///
///
/// ColumnKind classifies a column as either a scalar column (one value per
/// row) or an array column (a multidimensional array per row).  The value
/// is recorded in the AipsIO column descriptor header inside
/// `table.dat` and determines which storage manager read/write
/// path is taken at runtime.
///
/// Array columns carry additional shape information in
/// `ColumnDesc::ndim` and `ColumnDesc::shape`.  When
/// `ndim > 0` the shape is fixed across all rows; when
/// `ndim == -1` the shape is variable and stored per-cell in
/// indirect storage.
///
enum class ColumnKind : std::uint8_t {
    scalar = 0,
    array = 1,
};

///
/// Parsed column descriptor from a table.dat TableDesc section.
///
///
///
///
///
/// ColumnDesc holds every field that casacore serializes into an AipsIO
/// column descriptor block within `table.dat`.  It is produced
/// by `parse_table_dat_full()` and consumed by the storage
/// manager readers and by `Table::columns()`.
///
/// Key fields:
///
///   - `kind`       — scalar vs. array
///   - `type_string`— AipsIO class name, e.g.
///        `ScalarColumnDesc<Int     >`
///   - `data_type`  — DataType enum code
///   - `ndim`       — -1 (variable), 0 (scalar), >0 (fixed rank)
///   - `shape`      — fixed cell shape when ndim > 0
///   - `dm_type`    — storage manager type, e.g.
///        `StandardStMan`, `TiledShapeStMan`
///   - `dm_group`   — storage manager group name, used to
///        associate columns with the correct SM instance
///   - `keywords`   — per-column TableRecord (e.g. units, measures)
///
///
/// The `options` bitmask encodes column flags such as
/// Undefined/Direct/FixedShape as defined in the upstream casacore source.
///
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

///
/// Storage manager descriptor entry parsed from the post-TableDesc section
/// of table.dat.
///
///
///
///
///
/// After the TableDesc block in `table.dat`, casacore writes one
/// storage manager (SM) descriptor per SM instance in the table.  Each
/// descriptor names the SM class (`type_name`), assigns it a
/// monotonically increasing `sequence_number`, and carries a raw
/// AipsIO data blob (`data_blob`) whose interpretation is
/// SM-specific.
///
/// The `data_blob` is parsed lazily by the individual SM reader
/// (e.g. the SSM reader unpacks bucket counts and cache sizes from it).
/// Higher-level code should not inspect the blob directly; use the SM
/// reader APIs instead.
///
/// A `sequence_number` of 0 is assigned to the first SM; the
/// number increases by one for each additional SM in the file.
///
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

///
/// Per-column storage manager assignment parsed from the post-TableDesc
/// section of table.dat.
///
///
///
///
///
/// After the storage manager descriptors, `table.dat` contains
/// one `ColumnManagerSetup` per column, recording which SM
/// instance owns that column's data (identified by
/// `sequence_number`) and, for fixed-shape array columns, the
/// cell shape.
///
/// The `sequence_number` links back to a
/// `StorageManagerSetup` entry; the two vectors are parallel and
/// the numbers must match.
///
/// `has_shape` is true only for fixed-shape array columns
/// (`ColumnDesc::ndim > 0`).  Variable-shape columns store their
/// per-cell shape in indirect storage headers rather than here.
///
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

///
/// Full parsed TableDesc from a table.dat body.
///
///
///
///
///
/// TableDesc is the schema-level view of a casacore table: it records the
/// table name and comment, table-level keywords, private keywords (used for
/// hypercolumn definitions), and the ordered list of column descriptors.
///
/// It corresponds to the `TableDesc` AipsIO block that appears
/// early in the `table.dat` binary file, immediately after the
/// outer `Table` object header.
///
/// In casacore-mini, `TableDesc` is produced by
/// `parse_table_dat_full()` and exposed through
/// `Table::columns()` and `Table::keywords()`.  Direct
/// construction is only needed when writing new tables via
/// `Table::create()`.
///
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

///
/// Complete parsed representation of a casacore table.dat binary file.
///
///
///
///
/// @par Prerequisites
///   - `TableDesc`           — schema: columns and keywords
///   - `StorageManagerSetup` — one entry per SM instance
///   - `ColumnManagerSetup`  — one entry per column
///
///
///
/// Every casacore table directory contains a `table.dat` binary
/// file that encodes the complete table schema and storage manager metadata
/// using the AipsIO serialization format.  `TableDatFull` is the
/// in-memory representation produced by `parse_table_dat_full()`
/// and `read_table_dat_full()`.
///
/// The binary layout of `table.dat` is:
///
///   - Outer AipsIO object header (`Table`, version
///        `table_version`).
///   - Row count (`row_count`) and big-endian flag
///        (`big_endian`).
///   - Table type string (`table_type`), e.g.
///        `PlainTable`.
///   - Embedded `TableDesc` AipsIO block containing the schema
///        (`table_desc`).
///   - Post-TableDesc section:
///
///          - For tableVer 1: a second TableRecord for table keywords
///               (`table_keywords_v1`).
///          - Repeated row count (`post_td_row_count`).
///          - One `StorageManagerSetup` per SM instance.
///          - One `ColumnManagerSetup` per column.
///
///
///
/// For tableVer 2 and later, table-level keywords live in
/// `table_desc.keywords` instead of `table_keywords_v1`,
/// and the latter is empty.
///
/// @warning
/// The `storage_managers` and `column_setups` vectors are
/// parallel to each other in the sense that each
/// `ColumnManagerSetup::sequence_number` identifies an entry in
/// `storage_managers`.  They are NOT necessarily the same length.
///
///
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
/// @par Throws
///   - std::runtime_error if file cannot be opened or payload is malformed.
///
[[nodiscard]] TableDatFull read_table_dat_full(std::string_view table_dat_path);

/// @}
} // namespace casacore_mini
