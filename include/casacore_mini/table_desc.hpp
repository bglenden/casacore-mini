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

/// <summary>
/// casacore DataType enum values used in column descriptors.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// DataType maps directly to the integer codes stored in casacore AipsIO
/// column descriptors inside <src>table.dat</src>.  Each enumerator
/// corresponds to one of the fundamental casacore numeric or string types.
///
/// The underlying representation is <src>int32_t</src> to match the
/// wire format; the enumerator values are therefore fixed and must not
/// be renumbered.
///
/// Common values:
/// <ul>
///   <li> <src>tp_bool</src>    (0)  — boolean
///   <li> <src>tp_int</src>     (5)  — 32-bit signed integer
///   <li> <src>tp_float</src>   (7)  — 32-bit IEEE float
///   <li> <src>tp_double</src>  (8)  — 64-bit IEEE double
///   <li> <src>tp_complex</src> (9)  — complex<float>
///   <li> <src>tp_dcomplex</src>(10) — complex<double>
///   <li> <src>tp_string</src>  (11) — UTF-8 string
///   <li> <src>tp_table</src>   (12) — sub-table reference
///   <li> <src>tp_int64</src>   (29) — 64-bit signed integer
/// </ul>
///
/// Use <src>parse_data_type_name()</src> to map TaQL/TableDesc name strings
/// to this enum, and <src>data_type_to_string()</src> for the reverse.
/// </synopsis>
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

/// <summary>
/// Column descriptor kind distinguishing scalar from array columns.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// ColumnKind classifies a column as either a scalar column (one value per
/// row) or an array column (a multidimensional array per row).  The value
/// is recorded in the AipsIO column descriptor header inside
/// <src>table.dat</src> and determines which storage manager read/write
/// path is taken at runtime.
///
/// Array columns carry additional shape information in
/// <src>ColumnDesc::ndim</src> and <src>ColumnDesc::shape</src>.  When
/// <src>ndim > 0</src> the shape is fixed across all rows; when
/// <src>ndim == -1</src> the shape is variable and stored per-cell in
/// indirect storage.
/// </synopsis>
enum class ColumnKind : std::uint8_t {
    scalar = 0,
    array = 1,
};

/// <summary>
/// Parsed column descriptor from a table.dat TableDesc section.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// ColumnDesc holds every field that casacore serializes into an AipsIO
/// column descriptor block within <src>table.dat</src>.  It is produced
/// by <src>parse_table_dat_full()</src> and consumed by the storage
/// manager readers and by <src>Table::columns()</src>.
///
/// Key fields:
/// <ul>
///   <li> <src>kind</src>       — scalar vs. array
///   <li> <src>type_string</src>— AipsIO class name, e.g.
///        <src>ScalarColumnDesc<Int     ></src>
///   <li> <src>data_type</src>  — <linkto>DataType</linkto> enum code
///   <li> <src>ndim</src>       — -1 (variable), 0 (scalar), >0 (fixed rank)
///   <li> <src>shape</src>      — fixed cell shape when ndim > 0
///   <li> <src>dm_type</src>    — storage manager type, e.g.
///        <src>StandardStMan</src>, <src>TiledShapeStMan</src>
///   <li> <src>dm_group</src>   — storage manager group name, used to
///        associate columns with the correct SM instance
///   <li> <src>keywords</src>   — per-column TableRecord (e.g. units, measures)
/// </ul>
///
/// The <src>options</src> bitmask encodes column flags such as
/// Undefined/Direct/FixedShape as defined in the upstream casacore source.
/// </synopsis>
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

/// <summary>
/// Storage manager descriptor entry parsed from the post-TableDesc section
/// of table.dat.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// After the TableDesc block in <src>table.dat</src>, casacore writes one
/// storage manager (SM) descriptor per SM instance in the table.  Each
/// descriptor names the SM class (<src>type_name</src>), assigns it a
/// monotonically increasing <src>sequence_number</src>, and carries a raw
/// AipsIO data blob (<src>data_blob</src>) whose interpretation is
/// SM-specific.
///
/// The <src>data_blob</src> is parsed lazily by the individual SM reader
/// (e.g. the SSM reader unpacks bucket counts and cache sizes from it).
/// Higher-level code should not inspect the blob directly; use the SM
/// reader APIs instead.
///
/// A <src>sequence_number</src> of 0 is assigned to the first SM; the
/// number increases by one for each additional SM in the file.
/// </synopsis>
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

/// <summary>
/// Per-column storage manager assignment parsed from the post-TableDesc
/// section of table.dat.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// After the storage manager descriptors, <src>table.dat</src> contains
/// one <src>ColumnManagerSetup</src> per column, recording which SM
/// instance owns that column's data (identified by
/// <src>sequence_number</src>) and, for fixed-shape array columns, the
/// cell shape.
///
/// The <src>sequence_number</src> links back to a
/// <src>StorageManagerSetup</src> entry; the two vectors are parallel and
/// the numbers must match.
///
/// <src>has_shape</src> is true only for fixed-shape array columns
/// (<src>ColumnDesc::ndim > 0</src>).  Variable-shape columns store their
/// per-cell shape in indirect storage headers rather than here.
/// </synopsis>
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

/// <summary>
/// Full parsed TableDesc from a table.dat body.
/// </summary>
///
/// <use visibility=export>
///
/// <synopsis>
/// TableDesc is the schema-level view of a casacore table: it records the
/// table name and comment, table-level keywords, private keywords (used for
/// hypercolumn definitions), and the ordered list of column descriptors.
///
/// It corresponds to the <src>TableDesc</src> AipsIO block that appears
/// early in the <src>table.dat</src> binary file, immediately after the
/// outer <src>Table</src> object header.
///
/// In casacore-mini, <src>TableDesc</src> is produced by
/// <src>parse_table_dat_full()</src> and exposed through
/// <src>Table::columns()</src> and <src>Table::keywords()</src>.  Direct
/// construction is only needed when writing new tables via
/// <src>Table::create()</src>.
/// </synopsis>
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

/// <summary>
/// Complete parsed representation of a casacore table.dat binary file.
/// </summary>
///
/// <use visibility=export>
///
/// <prerequisite>
///   <li> <src>TableDesc</src>           — schema: columns and keywords
///   <li> <src>StorageManagerSetup</src> — one entry per SM instance
///   <li> <src>ColumnManagerSetup</src>  — one entry per column
/// </prerequisite>
///
/// <synopsis>
/// Every casacore table directory contains a <src>table.dat</src> binary
/// file that encodes the complete table schema and storage manager metadata
/// using the AipsIO serialization format.  <src>TableDatFull</src> is the
/// in-memory representation produced by <src>parse_table_dat_full()</src>
/// and <src>read_table_dat_full()</src>.
///
/// The binary layout of <src>table.dat</src> is:
/// <ol>
///   <li> Outer AipsIO object header (<src>Table</src>, version
///        <src>table_version</src>).
///   <li> Row count (<src>row_count</src>) and big-endian flag
///        (<src>big_endian</src>).
///   <li> Table type string (<src>table_type</src>), e.g.
///        <src>PlainTable</src>.
///   <li> Embedded <src>TableDesc</src> AipsIO block containing the schema
///        (<src>table_desc</src>).
///   <li> Post-TableDesc section:
///        <ul>
///          <li> For tableVer 1: a second TableRecord for table keywords
///               (<src>table_keywords_v1</src>).
///          <li> Repeated row count (<src>post_td_row_count</src>).
///          <li> One <src>StorageManagerSetup</src> per SM instance.
///          <li> One <src>ColumnManagerSetup</src> per column.
///        </ul>
/// </ol>
///
/// For tableVer 2 and later, table-level keywords live in
/// <src>table_desc.keywords</src> instead of <src>table_keywords_v1</src>,
/// and the latter is empty.
///
/// <note role="caution">
/// The <src>storage_managers</src> and <src>column_setups</src> vectors are
/// parallel to each other in the sense that each
/// <src>ColumnManagerSetup::sequence_number</src> identifies an entry in
/// <src>storage_managers</src>.  They are NOT necessarily the same length.
/// </note>
/// </synopsis>
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
/// <thrown><li>std::runtime_error on malformed input.</thrown>
[[nodiscard]] TableDatFull parse_table_dat_full(std::span<const std::uint8_t> bytes);

/// Read and parse full table.dat from a file path.
///
/// <thrown>
///   <li>std::runtime_error if file cannot be opened or payload is malformed.
/// </thrown>
[[nodiscard]] TableDatFull read_table_dat_full(std::string_view table_dat_path);

} // namespace casacore_mini
