// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Columns index — fast key lookup on scalar columns.

/// 
/// Builds an in-memory index on one or more scalar columns for fast row lookup.
/// 
///
///
///
/// 
/// `ColumnsIndex` pre-reads all values for the nominated key columns into a
/// sorted `std::map` that maps composite key vectors to lists of matching row
/// indices.  After construction the index supports O(log N) lookup by key.
///
/// Both unique and non-unique keys are supported:
/// - `get_row_numbers()` returns all matching rows.
/// - `get_row_number()` returns exactly one row or throws if the key is absent
///   or maps to multiple rows.
/// - `contains()` tests existence without returning rows.
///
/// `rebuild()` re-scans the table to refresh the index after rows are added
/// or modified.
///
/// Keys are represented as `std::vector<CellValue>` (one element per key
/// column) and compared via a custom lexicographic `KeyCompare` functor that
/// respects the variant ordering of `CellValue`.
/// 
///
/// @par Example
/// @code{.cpp}
///   Table ms = Table::open("my_vis.ms");
///   ColumnsIndex idx(ms, {"SCAN_NUMBER"});
///
///   Record key;
///   key.fields["SCAN_NUMBER"] = RecordValue(std::int32_t{3});
///   auto rows = idx.get_row_numbers(key);
///   // rows contains all row indices where SCAN_NUMBER == 3
/// @endcode
/// 
///
/// @par Motivation
/// Repeated lookup of rows by key (e.g., finding all baselines for a given
/// scan) would otherwise require a full table scan per query.  Pre-building
/// the index pays the O(N) scan cost once and amortises it over many queries.
/// 
class ColumnsIndex {
  public:
    /// Build an index on the given key columns.
    ColumnsIndex(Table& table, std::vector<std::string> key_columns);

    /// Find row(s) matching a key. The key Record must have fields matching
    /// the key column names. Returns all matching row indices.
    [[nodiscard]] std::vector<std::uint64_t> get_row_numbers(const Record& key) const;

    /// Find a single row matching a unique key. Returns the row index, or
    /// throws if the key is not found or is not unique.
    [[nodiscard]] std::uint64_t get_row_number(const Record& key) const;

    /// Check if a key exists in the index.
    [[nodiscard]] bool contains(const Record& key) const;

    /// Rebuild the index (e.g., after the table has been modified).
    void rebuild();

    /// Number of unique keys in the index.
    [[nodiscard]] std::size_t unique_key_count() const;

  private:
    Table* table_;
    std::vector<std::string> key_columns_;

    // Key is a vector of CellValue (one per key column), mapped to row indices.
    using IndexKey = std::vector<CellValue>;
    struct KeyCompare {
        bool operator()(const IndexKey& a, const IndexKey& b) const;
    };
    std::map<IndexKey, std::vector<std::uint64_t>, KeyCompare> index_;

    void build();
    [[nodiscard]] IndexKey make_key(const Record& rec) const;
    [[nodiscard]] IndexKey make_key_from_row(std::uint64_t row) const;
};

} // namespace casacore_mini
