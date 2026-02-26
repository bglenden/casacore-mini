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

/// @file
/// @brief High-level Table abstraction over casacore table directories.
///
/// Provides the fundamental user-facing API: tables, columns, rows, keywords.
/// Users never interact directly with storage managers -- the Table class
/// manages SM reader/writer lifecycle internally.

class Table;

/// Describes a column for Table::create().
struct TableColumnSpec {
    std::string name;
    DataType data_type = DataType::tp_int;
    ColumnKind kind = ColumnKind::scalar;
    std::vector<std::int64_t> shape; // for array columns
    std::string comment;
};

/// Options for Table::create() that control which storage manager backs the table.
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

/// Read a row as a Record (field names = column names).
///
/// Provides row-oriented generic access. Efficient for inspecting entire rows
/// or when column names aren't known at compile time.
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

/// The fundamental table abstraction.
///
/// Wraps a casacore table directory and provides high-level access to
/// rows, columns, and keywords. Storage manager details are hidden.
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
