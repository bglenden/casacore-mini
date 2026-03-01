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
/// Sorts the table by one or more key columns and iterates over groups of
/// consecutive rows with identical key values. Each iteration step returns
/// a RefTable containing the rows for that group.
///
/// Usage:
/// @code
///   TableIterator iter(table, {"FIELD_ID", "SCAN_NUMBER"});
///   while (iter.next()) {
///       auto& group = iter.current();
///       // group is a RefTable with rows sharing the same FIELD_ID+SCAN_NUMBER
///   }
/// @endcode
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
