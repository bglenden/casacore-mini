// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/ref_table.hpp"
#include "casacore_mini/table.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Table iterator — groups rows by key column values.

/// 
/// Groups table rows by one or more key column values and iterates over the groups.
/// 
///
///
///
/// 
/// `TableIterator` sorts the table by one or more key columns and iterates
/// over contiguous groups of rows that share identical key values.  Each call
/// to `next()` advances to the next group and makes a `RefTable` view of those
/// rows available via `current()`.
///
/// The sort is performed once during construction (via `build_sorted_rows()`).
/// Subsequent calls to `next()` scan the sorted row list to locate group
/// boundaries using `keys_equal()`.
///
/// Calling `reset()` returns the iterator to the beginning without re-sorting.
///
/// Groups are emitted in sorted-key order, which matches casacore's
/// `TableIterator` default behavior with `Order::Ascending`.
/// 
///
/// @par Example
/// @code{.cpp}
///   Table ms = Table::open("my_vis.ms");
///   TableIterator iter(ms, {"FIELD_ID", "SCAN_NUMBER"});
///   while (iter.next()) {
///       const RefTable& group = iter.current();
///       // group contains rows sharing the same FIELD_ID and SCAN_NUMBER
///       std::cout << "group nrow=" << group.nrow() << "\n";
///   }
/// @endcode
/// 
///
/// @par Motivation
/// Many visibility processing algorithms iterate over baselines, fields, or
/// scans independently.  `TableIterator` provides this grouping without
/// requiring the caller to manage row sorting and boundary detection manually.
/// 
class TableIterator {
  public:
    /// Construct an iterator that groups by the given columns.
    TableIterator(Table& table, std::vector<std::string> key_columns);

    /// Advance to the next group. Returns false when exhausted.
    bool next();

    /// Reset to the beginning.
    void reset();

    /// Get the current group as a RefTable.
    /// Only valid after a successful next() call.
    [[nodiscard]] const RefTable& current() const;
    [[nodiscard]] RefTable& current();

    /// Check if iteration is exhausted.
    [[nodiscard]] bool at_end() const;

  private:
    Table* table_;
    std::vector<std::string> key_columns_;
    std::vector<std::uint64_t> sorted_rows_;
    std::size_t group_start_ = 0;
    std::size_t group_end_ = 0;
    bool started_ = false;
    std::optional<RefTable> current_group_;

    void build_sorted_rows();
    bool keys_equal(std::uint64_t row_a, std::uint64_t row_b) const;
};

} // namespace casacore_mini
