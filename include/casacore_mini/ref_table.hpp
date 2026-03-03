// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Reference table (view) — an ephemeral subset of rows/columns.

/// <summary>
/// An ephemeral, in-memory view over a subset of rows from a base `Table`.
/// </summary>
///
/// <use visibility=export/>
///
/// <synopsis>
/// A `RefTable` wraps an existing `Table` and presents a filtered view of a
/// subset of its rows identified by a `std::vector<uint64_t>` index mapping.
/// It does not persist to disk and holds no data of its own; all cell reads
/// and writes are delegated to the base table through the row mapping.
///
/// Three construction modes are provided:
/// - From an explicit list of base-table row indices.
/// - From a boolean mask (selects rows where `mask[i]` is true).
/// - Empty (for progressive population via `add_row()`).
///
/// `base_row(view_row)` translates a view-local row number to the corresponding
/// base-table row, allowing callers to pass the mapped index to direct
/// base-table APIs.
///
/// `RefTable` is the return type of `TableIterator::current()` and of
/// TaQL query results.
/// </synopsis>
///
/// <example>
/// <srcblock>
///   Table ms = Table::open("my_vis.ms");
///   // Select rows where ANTENNA1 == 0
///   std::vector<uint64_t> rows;
///   for (uint64_t r = 0; r < ms.nrow(); ++r) {
///       if (std::get<int32_t>(ms.read_scalar_cell("ANTENNA1", r)) == 0)
///           rows.push_back(r);
///   }
///   RefTable view(ms, std::move(rows));
///   std::cout << "selected " << view.nrow() << " rows\n";
///   auto val = view.read_scalar_cell("ANTENNA2", 0);
/// </srcblock>
/// </example>
class RefTable {
  public:
    /// Construct a view with selected rows.
    RefTable(Table& base, std::vector<std::uint64_t> row_indices);

    /// Construct a view from a boolean mask (selects rows where mask[i] is true).
    RefTable(Table& base, const std::vector<bool>& row_mask);

    /// Construct an empty view (for progressive population).
    explicit RefTable(Table& base);

    /// Number of rows in the view.
    [[nodiscard]] std::uint64_t nrow() const;

    /// Number of columns (same as base table, or projected subset).
    [[nodiscard]] std::size_t ncolumn() const;

    /// Column descriptors (from base table).
    [[nodiscard]] const std::vector<ColumnDesc>& columns() const;

    /// Table-level keywords (from base table).
    [[nodiscard]] const Record& keywords() const;

    /// Table name (base table name).
    [[nodiscard]] std::string table_name() const;

    /// Read a scalar cell through the row mapping.
    [[nodiscard]] CellValue read_scalar_cell(std::string_view col_name, std::uint64_t row) const;

    /// Read a double array cell through the row mapping.
    [[nodiscard]] std::vector<double> read_array_double_cell(std::string_view col_name,
                                                             std::uint64_t row) const;

    /// Read a float array cell through the row mapping.
    [[nodiscard]] std::vector<float> read_array_float_cell(std::string_view col_name,
                                                           std::uint64_t row) const;

    /// Read an int array cell through the row mapping.
    [[nodiscard]] std::vector<std::int32_t> read_array_int_cell(std::string_view col_name,
                                                                std::uint64_t row) const;

    /// Get the mapped base-table row indices.
    [[nodiscard]] const std::vector<std::uint64_t>& row_indices() const;

    /// Map a view row to the base-table row.
    [[nodiscard]] std::uint64_t base_row(std::uint64_t view_row) const;

    /// Add a row index to the view.
    void add_row(std::uint64_t base_row_index);

    /// Access the underlying base table.
    [[nodiscard]] Table& base_table();
    [[nodiscard]] const Table& base_table() const;

  private:
    Table* base_;
    std::vector<std::uint64_t> rows_;
};

} // namespace casacore_mini
