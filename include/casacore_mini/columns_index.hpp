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
/// Builds an in-memory index on one or more scalar columns for fast row
/// lookup by key value. Supports unique and non-unique keys.
///
/// Usage:
/// @code
///   ColumnsIndex idx(table, {"SCAN_NUMBER"});
///   auto rows = idx.get_row_numbers(Record{{"SCAN_NUMBER", RecordValue(1)}});
///   // rows contains all row indices where SCAN_NUMBER == 1
/// @endcode
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
