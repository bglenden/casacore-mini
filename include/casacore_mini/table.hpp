// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/record.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_directory.hpp"

#include <complex>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace casacore_mini {

// ---------------------------------------------------------------------------
// Table-level lock model
// ---------------------------------------------------------------------------

///
/// Lock mode for table access, mirroring upstream casacore's TableLock options.
///
///
///
///
///
/// TableLockMode controls how a Table acquires and releases
/// the advisory lock on its table directory.  casacore-mini implements
/// locking through a simple lock-file (`table.lock`) rather than
/// the full inter-process synchronisation used by upstream casacore.
///
/// Modes:
///
///   - `NoLock`       — No lock file is created or checked.
///        Suitable for read-only access when concurrent writers are known
///        to be absent.
///   - `AutoLock`     — The table acquires a lock before each
///        read or write operation and releases it after `flush()`.
///   - `PermanentLock`— A single lock is taken at open time and
///        held until the Table object is destroyed or explicitly unlocked.
///   - `UserLock`     — The caller manages lock acquisition and
///        release manually via `Table::lock()` and
///        `Table::unlock()`.
///
///
enum class TableLockMode : std::uint8_t {
    NoLock,        ///< No locking (default for read-only tables).
    AutoLock,      ///< Lock automatically on read/write, release on flush.
    PermanentLock, ///< Lock once, hold until close.
    UserLock,      ///< Lock/unlock manually via lock()/unlock().
};

///
/// Type and subtype metadata read from a table's table.info file.
///
///
///
///
///
/// Every casacore table directory contains a plain-text `table.info`
/// file with two lines of the form:
/// @code{.cpp}
///   Type = MeasurementSet
///   SubType = 2
/// @endcode
///
/// TableInfo captures those two fields.  Use `Table::table_info()`
/// to retrieve them and `Table::set_table_info()` to update them.
/// The `type` field is the casacore table class name (e.g.
/// `MeasurementSet`, `ANTENNA`); `sub_type`
/// carries an optional secondary tag (often a version string or empty).
///
struct TableInfo {
    std::string type{};     ///< The "Type = ..." value.
    std::string sub_type{}; ///< The "SubType = ..." value.
};

// ---------------------------------------------------------------------------
// Utility: data type <-> string conversion
// ---------------------------------------------------------------------------

/// Parse a data type name string to a DataType enum.
///
/// Accepts the casacore type names used in TaQL and table descriptors:
/// "BOOL", "Bool", "B", "INT", "Int", "I4", "INT32", "I", "INT64", "I8",
/// "SHORT", "Short", "I2", "FLOAT", "Float", "R4", "DOUBLE", "Double",
/// "R8", "D", "COMPLEX", "Complex", "C4", "DCOMPLEX", "DComplex", "C8",
/// "STRING", "String", "S".
///
/// @throws std::runtime_error if the name is not recognized.
[[nodiscard]] DataType parse_data_type_name(std::string_view name);

/// Convert a DataType enum to a canonical string name.
/// Returns names like "Bool", "Int", "Float", "Double", "Complex",
/// "DComplex", "String", "Int64", "Short".
[[nodiscard]] std::string data_type_to_string(DataType dt);

/// @file
/// @brief High-level Table abstraction over casacore table directories.
///
/// Provides the fundamental user-facing API: tables, columns, rows, keywords.
/// Users never interact directly with storage managers -- the Table class
/// manages SM reader/writer lifecycle internally.

class Table;

///
/// Column specification used when creating a new Table.
///
///
///
///
///
/// TableColumnSpec describes one column passed to `Table::create()`.
/// Each field maps directly to a field in the AipsIO column descriptor
/// written into `table.dat`.
///
/// For scalar columns, leave `shape` empty and set
/// `kind = ColumnKind::scalar`.  For fixed-shape array columns,
/// populate `shape` and set `kind = ColumnKind::array`.
/// Variable-shape array columns are also expressed with
/// `kind = ColumnKind::array` but with an empty `shape`;
/// cell shapes are then provided at write time via
/// `Table::write_indirect_array()`.
///
///
/// @par Example
/// @code{.cpp}
///   // Scalar double column
///   TableColumnSpec time_col;
///   time_col.name      = "TIME";
///   time_col.data_type = DataType::tp_double;
///   time_col.kind      = ColumnKind::scalar;
///
///   // Fixed-shape float array column (4 polarisations x 64 channels)
///   TableColumnSpec data_col;
///   data_col.name      = "DATA";
///   data_col.data_type = DataType::tp_complex;
///   data_col.kind      = ColumnKind::array;
///   data_col.shape     = {4, 64};
/// @endcode
///
struct TableColumnSpec {
    std::string name;
    DataType data_type = DataType::tp_int;
    ColumnKind kind = ColumnKind::scalar;
    std::vector<std::int64_t> shape{}; // for array columns
    std::string comment{};
};

///
/// Options controlling the storage manager and metadata for Table::create().
///
///
///
///
///
/// TableCreateOptions lets callers override the default storage manager
/// selection and embed initial table-level metadata when calling
/// `Table::create()`.
///
/// The default configuration uses `StandardStMan` with big-endian
/// byte order, which is compatible with all casacore readers.
///
/// Fields:
///
///   - `sm_type`        — storage manager class name, e.g.
///        `StandardStMan` or `TiledShapeStMan`.
///   - `sm_group`       — SM group name; must match the
///        `dm_group` in the column descriptors.
///   - `big_endian`     — byte order flag written into
///        `table.dat`.
///   - `tile_shape`     — cell tile dimensions for tiled SMs;
///        ignored for `StandardStMan`.
///   - `table_keywords` — table-level keywords embedded in the
///        `TableDesc` section of `table.dat`.
///   - `column_keywords`— per-column keywords; the vector must
///        be either empty or parallel to the columns vector passed to
///        `Table::create()`.
///   - `private_keywords`— private TableRecord (e.g. hypercolumn
///        definitions required by `TiledDataStMan`).
///
///
struct TableCreateOptions {
    std::string sm_type = "StandardStMan";
    std::string sm_group = "StandardStMan";
    bool big_endian = true;
    std::vector<std::int64_t> tile_shape; // for tiled SMs

    /// Table-level keywords to embed.
    Record table_keywords;
    /// Column-level keywords, indexed parallel to the columns vector.
    std::vector<Record> column_keywords;
    /// Private keywords (e.g. hypercolumn definitions for TiledDataStMan).
    Record private_keywords;
};

///
/// Row-oriented accessor that reads or writes an entire table row as a Record.
///
///
///
///
/// @par Prerequisites
///   - `Table`       — the table being accessed
///   - `Record`      — the row representation (field = column name)
///   - `RecordValue` — per-cell value type
///
///
///
/// TableRow provides a convenient interface for reading or writing complete
/// rows without needing to call column-specific cell methods individually.
/// It is most useful when:
///
///   - column names are not known at compile time, or
///   - an entire row must be inspected or copied at once.
///
///
/// On construction, the TableRow captures a reference to the owning
/// `Table` and the set of column names to include.  The
/// two-argument constructor accepts an explicit list; the single-argument
/// constructor reads all scalar columns present in the table.
///
/// `get(row)` returns a Record whose field names
/// are the selected column names and whose values are scalar
/// `RecordValue` objects.  `put(row, values)` writes a
/// Record back; field names must match column names exactly.
///
/// @warning
/// TableRow holds a raw pointer to the Table.  The Table must outlive all
/// TableRow objects that reference it.
///
///
///
/// @par Example
/// @code{.cpp}
///   Table tbl = Table::open("my.tbl");
///   TableRow row(tbl);
///
///   for (std::uint64_t r = 0; r < tbl.nrow(); ++r) {
///       Record rec = row.get(r);
///       // rec["TIME"] holds a RecordValue(double)
///   }
/// @endcode
///
class TableRow {
  public:
    /// Construct a TableRow that reads all scalar columns.
    explicit TableRow(Table& table);

    /// Construct a TableRow that reads a subset of columns.
    TableRow(Table& table, const std::vector<std::string>& column_names);

    /// Get entire row as a Record.
    /// Field names are column names; values are RecordValue scalars.
    [[nodiscard]] Record get(std::uint64_t row) const;

    /// Write a Record as a row. Field names must match column names.
    void put(std::uint64_t row, const Record& values);

  private:
    Table* table_;
    std::vector<std::string> column_names_;
};

///
/// The fundamental casacore-mini table abstraction.
///
///
///
///
/// @par Prerequisites
///   - `TableColumnSpec` — column descriptor for creation
///   - `Record`          — keyword storage
///   - `TableDesc`       — parsed schema
///
///
///
/// Table wraps a casacore table directory and provides high-level access to
/// rows, columns, and keywords.  Storage manager details (StandardStMan,
/// TiledShapeStMan, IncrementalStMan) are hidden behind a uniform cell-level
/// read/write API.
///
/// A casacore table directory contains:
///
///   - `table.dat`  — binary schema and storage manager metadata
///   - `table.info` — type/subtype text file
///   - One or more storage manager files (e.g. `table.f0` for SSM)
///
///
/// The three factory methods cover the common access patterns:
///
///   - `Table::open()`    — read-only access to an existing table
///   - `Table::open_rw()` — read-write access to an existing table
///   - `Table::create()`  — create a new table with a given column schema
///
///
/// Tables are reference-counted via `shared_ptr` internally, so
/// copies of a Table object refer to the same underlying state.
///
/// Cell-level access is type-specific; choose the method that matches the
/// column's `DataType`:
///
///   - Scalar: `read_scalar_cell()`, `write_scalar_cell()`
///   - Float array: `read_array_float_cell()`, `write_array_float_cell()`
///   - Double array: `read_array_double_cell()`, `write_array_double_cell()`
///   - Complex array: `read_array_complex_cell()`, `write_array_complex_cell()`
///   - Variable-shape: `write_indirect_array()`, `cell_shape()`
///
///
///
/// @par Example
/// Create a two-column table, add rows, and read them back:
/// @code{.cpp}
///   using namespace casacore_mini;
///
///   // Create
///   auto tbl = Table::create("my.tbl", {
///       {"TIME",   DataType::tp_double, ColumnKind::scalar},
///       {"FLUX",   DataType::tp_float,  ColumnKind::scalar},
///   });
///   tbl.add_row(3);
///   tbl.write_scalar_cell("TIME", 0, CellValue(1.0));
///   tbl.write_scalar_cell("FLUX", 0, CellValue(0.5f));
///   tbl.flush();
///
///   // Open read-only
///   auto ro = Table::open("my.tbl");
///   auto val = ro.read_scalar_cell("TIME", 0);
/// @endcode
///
///
/// @par Motivation
/// casacore tables use a complex binary format with pluggable storage
/// managers.  Table hides that complexity behind a stable API that can
/// read and write any SM type without exposing SM internals to callers.
///
class Table {
  public:
    /// Open an existing table from a directory path (read-only).
    [[nodiscard]] static Table open(const std::filesystem::path& path);

    /// Open an existing table for read-write.
    /// Copies existing SSM data from reader to writer so cells can be modified.
    [[nodiscard]] static Table open_rw(const std::filesystem::path& path);

    /// Create a new table with the given columns.
    /// Uses SSM (StandardStMan) by default.
    [[nodiscard]] static Table create(const std::filesystem::path& path,
                                      const std::vector<TableColumnSpec>& columns,
                                      std::uint64_t nrows = 0);

    /// Create a new table with explicit storage manager options.
    [[nodiscard]] static Table create(const std::filesystem::path& path,
                                      const std::vector<TableColumnSpec>& columns,
                                      std::uint64_t nrows, const TableCreateOptions& options);

    /// Number of rows in the table.
    [[nodiscard]] std::uint64_t nrow() const;

    /// Number of columns.
    [[nodiscard]] std::size_t ncolumn() const;

    /// Column descriptors.
    [[nodiscard]] const std::vector<ColumnDesc>& columns() const;

    /// Table-level keywords.
    [[nodiscard]] const Record& keywords() const;

    /// Mutable table-level keywords.
    [[nodiscard]] Record& rw_keywords();

    /// Table name (directory basename).
    [[nodiscard]] std::string table_name() const;

    /// Path to the table directory.
    [[nodiscard]] const std::filesystem::path& path() const;

    /// Flush pending writes to disk.
    void flush();

    /// Whether this table is writable.
    [[nodiscard]] bool is_writable() const;

    /// Add rows to the end of the table (default-initialized).
    /// Requires a writable table.
    void add_row(std::uint64_t n = 1);

    /// Remove a single row. Subsequent rows shift down by one.
    /// Requires a writable table.
    void remove_row(std::uint64_t row);

    // -- Cell-level read/write API --

    /// Read a scalar cell value from any SM.
    [[nodiscard]] CellValue read_scalar_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read an array cell as doubles from any SM.
    [[nodiscard]] std::vector<double> read_array_double_cell(std::string_view col_name,
                                                             std::uint64_t row) const;

    /// Read an array cell as floats from any SM.
    [[nodiscard]] std::vector<float> read_array_float_cell(std::string_view col_name,
                                                           std::uint64_t row) const;

    /// Read an array cell as bools from any SM.
    [[nodiscard]] std::vector<bool> read_array_bool_cell(std::string_view col_name,
                                                         std::uint64_t row) const;

    /// Read an array cell as int32_t from any SM (SSM or TSM).
    [[nodiscard]] std::vector<std::int32_t> read_array_int_cell(std::string_view col_name,
                                                                std::uint64_t row) const;

    /// Read an array cell as complex<float> from any SM.
    [[nodiscard]] std::vector<std::complex<float>>
    read_array_complex_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read an array cell as complex<double> from any SM.
    [[nodiscard]] std::vector<std::complex<double>>
    read_array_dcomplex_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read an array cell as strings from SSM indirect storage.
    [[nodiscard]] std::vector<std::string> read_array_string_cell(std::string_view col_name,
                                                                  std::uint64_t row) const;

    /// Read an array cell as raw bytes from any SM.
    [[nodiscard]] std::vector<std::uint8_t> read_array_raw_cell(std::string_view col_name,
                                                                std::uint64_t row) const;

    /// Get the cell shape for a variable-shape array column (from indirect header).
    [[nodiscard]] std::vector<std::int64_t> cell_shape(std::string_view col_name,
                                                       std::uint64_t row) const;

    /// Whether the table is big-endian.
    [[nodiscard]] bool is_big_endian() const;

    /// Write a scalar cell value via the appropriate SM.
    void write_scalar_cell(std::string_view col_name, std::uint64_t row, const CellValue& value);

    /// Write a float array cell.
    void write_array_float_cell(std::string_view col_name, std::uint64_t row,
                                const std::vector<float>& values);

    /// Write a double array cell.
    void write_array_double_cell(std::string_view col_name, std::uint64_t row,
                                 const std::vector<double>& values);

    /// Write a bool array cell (for indirect/variable-shape bool arrays).
    void write_array_bool_cell(std::string_view col_name, std::uint64_t row,
                               const std::vector<bool>& values,
                               const std::vector<std::int32_t>& shape);

    /// Write a complex float array cell (for indirect/variable-shape).
    void write_array_complex_cell(std::string_view col_name, std::uint64_t row,
                                  const std::vector<std::complex<float>>& values,
                                  const std::vector<std::int32_t>& shape);

    /// Write an int32_t array cell (routes to TSM writer).
    void write_array_int_cell(std::string_view col_name, std::uint64_t row,
                              const std::vector<std::int32_t>& values);

    /// Write raw bytes for an array cell (routes to TSM writer).
    void write_array_raw_cell(std::string_view col_name, std::uint64_t row,
                              const std::vector<std::uint8_t>& data);

    /// Write a variable-shape array as raw bytes via SSM indirect storage.
    void write_indirect_array(std::string_view col_name, std::uint64_t row,
                              const std::vector<std::int32_t>& shape,
                              const std::vector<std::uint8_t>& data);

    /// Find the ColumnDesc for a column by name.
    [[nodiscard]] const ColumnDesc* find_column_desc(std::string_view name) const;

    /// Find column index in the TableDesc columns vector.
    [[nodiscard]] std::size_t find_column_index(std::string_view name) const;

    // -- Table info / metadata --

    /// Read the table.info type and subtype.
    [[nodiscard]] TableInfo table_info() const;

    /// Get the table type string (convenience for table_info().type).
    [[nodiscard]] std::string table_info_type() const;

    /// Set the table.info type and subtype (writes to disk immediately).
    void set_table_info(std::string_view type_string, std::string_view sub_type = "");

    /// Access private keywords (e.g. hypercolumn definitions).
    [[nodiscard]] const Record& private_keywords() const;

    /// Mutable access to private keywords.
    [[nodiscard]] Record& rw_private_keywords();

    /// Check if a column exists by name.
    [[nodiscard]] bool has_column(std::string_view name) const;

    // -- Lock model --

    /// Acquire a lock on the table. Creates/updates the lock file.
    /// @param write  True for write lock, false for read lock.
    void lock(bool write = true);

    /// Release the table lock.
    void unlock();

    /// Check if this Table instance currently holds a lock.
    [[nodiscard]] bool has_lock() const;

    /// Check if the table's lock file indicates it is locked (by any process).
    [[nodiscard]] bool is_locked() const;

    /// Get the lock mode.
    [[nodiscard]] TableLockMode lock_mode() const;

    /// Set the lock mode.
    void set_lock_mode(TableLockMode mode);

    // -- Static utilities --

    /// Drop (delete) a table directory from disk.
    /// @param path  Path to the table directory.
    /// @param force Skip lock checks and remove even if lock metadata exists.
    /// @return True if successfully removed, false if path doesn't exist.
    /// @throws std::runtime_error if the table is locked (unless force is true).
    [[nodiscard]] static bool drop_table(const std::filesystem::path& path, bool force = false);

  private:
    Table() = default;

    // -- Low-level internals (not for MS consumer code) --

    /// Access the underlying TableDirectory.
    [[nodiscard]] TableDirectory& directory();
    [[nodiscard]] const TableDirectory& directory() const;

    /// Access the parsed table.dat.
    [[nodiscard]] const TableDatFull& table_dat() const;
    [[nodiscard]] TableDatFull& rw_table_dat();

    struct Impl;
    std::shared_ptr<Impl> impl_;

    // Allow MeasurementSet and its internals to access directory/table_dat
    // for table construction and flush.
    friend class MeasurementSet;
    friend class MsWriter;
};

} // namespace casacore_mini
