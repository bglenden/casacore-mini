// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Concatenated table — virtual concatenation of multiple tables.
/// @ingroup tables
/// @addtogroup tables
/// @{

///
/// Presents multiple tables with the same schema as a single virtual table.
///
///
///
///
///
/// `ConcatTable` provides a row-union view over a list of `Table` objects
/// that share the same column schema.  Row N in the concatenated view is
/// mapped to a specific `(table_index, local_row)` pair via a cumulative
/// row-count array, so reads are O(1) dispatched.
///
/// Keywords are taken from the first component table; the remaining tables'
/// keywords are not consulted.  Column descriptors are also taken from the
/// first table.
///
/// Only read operations are currently supported via `read_scalar_cell` and
/// `read_array_double_cell`.  Write operations must be performed through the
/// individual component `Table` objects directly.
///
/// Use `decompose_row()` to translate a global row index into the component
/// table index and local row for low-level access.
///
///
/// @par Example
/// @code{.cpp}
///   Table t1 = Table::open("ms_part1.ms");
///   Table t2 = Table::open("ms_part2.ms");
///   ConcatTable ct({&t1, &t2});
///   std::cout << "total rows: " << ct.nrow() << "\n";
///   auto val = ct.read_scalar_cell("ANTENNA1", ct.nrow() - 1);
/// @endcode
///
///
/// @par Motivation
/// Processing large measurement sets that have been split into per-chunk
/// sub-tables benefits from a unified virtual table that routes row requests
/// transparently to the correct chunk.
///
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

/// @}
} // namespace casacore_mini
