#pragma once

#include "casacore_mini/table.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Concatenated table — virtual concatenation of multiple tables.
///
/// Presents multiple tables with the same schema as a single virtual table.
/// Row N in the concatenated view maps to a specific (table_index, local_row).
/// Keywords are taken from the first table.
///
/// Usage:
/// @code
///   ConcatTable ct({&table1, &table2, &table3});
///   auto val = ct.read_scalar_cell("COL", 5);  // routes to correct table
/// @endcode
class ConcatTable {
  public:
    /// Construct from a list of tables (must have same column schema).
    explicit ConcatTable(std::vector<Table*> tables);

    /// Total number of rows across all tables.
    [[nodiscard]] std::uint64_t nrow() const;

    /// Number of columns (from first table).
    [[nodiscard]] std::size_t ncolumn() const;

    /// Column descriptors (from first table).
    [[nodiscard]] const std::vector<ColumnDesc>& columns() const;

    /// Keywords (from first table).
    [[nodiscard]] const Record& keywords() const;

    /// Read a scalar cell through the row mapping.
    [[nodiscard]] CellValue read_scalar_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read a double array cell through the row mapping.
    [[nodiscard]] std::vector<double> read_array_double_cell(std::string_view col_name,
                                                              std::uint64_t row) const;

    /// Number of component tables.
    [[nodiscard]] std::size_t table_count() const;

    /// Map a global row to (table_index, local_row).
    [[nodiscard]] std::pair<std::size_t, std::uint64_t> decompose_row(std::uint64_t row) const;

    /// Access a component table by index.
    [[nodiscard]] Table& table_at(std::size_t index);
    [[nodiscard]] const Table& table_at(std::size_t index) const;

  private:
    std::vector<Table*> tables_;
    std::vector<std::uint64_t> cumulative_rows_; // cumulative row counts
    std::uint64_t total_rows_ = 0;
};

} // namespace casacore_mini
