// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/concat_table.hpp"

#include <stdexcept>

namespace casacore_mini {

ConcatTable::ConcatTable(std::vector<Table*> tables) : tables_(std::move(tables)) {
    if (tables_.empty()) {
        throw std::runtime_error("ConcatTable: must have at least one table");
    }
    cumulative_rows_.reserve(tables_.size());
    std::uint64_t cumulative = 0;
    for (auto* t : tables_) {
        cumulative_rows_.push_back(cumulative);
        cumulative += t->nrow();
    }
    total_rows_ = cumulative;
}

std::uint64_t ConcatTable::nrow() const {
    return total_rows_;
}

std::size_t ConcatTable::ncolumn() const {
    return tables_[0]->ncolumn();
}

const std::vector<ColumnDesc>& ConcatTable::columns() const {
    return tables_[0]->columns();
}

const Record& ConcatTable::keywords() const {
    return tables_[0]->keywords();
}

CellValue ConcatTable::read_scalar_cell(std::string_view col_name, std::uint64_t row) const {
    auto [ti, lr] = decompose_row(row);
    return tables_[ti]->read_scalar_cell(col_name, lr);
}

std::vector<double> ConcatTable::read_array_double_cell(std::string_view col_name,
                                                        std::uint64_t row) const {
    auto [ti, lr] = decompose_row(row);
    return tables_[ti]->read_array_double_cell(col_name, lr);
}

std::size_t ConcatTable::table_count() const {
    return tables_.size();
}

std::pair<std::size_t, std::uint64_t> ConcatTable::decompose_row(std::uint64_t row) const {
    if (row >= total_rows_) {
        throw std::out_of_range("ConcatTable: row " + std::to_string(row) +
                                " out of range (total=" + std::to_string(total_rows_) + ")");
    }
    // Find the table that contains this row.
    for (std::size_t i = tables_.size() - 1; i > 0; --i) {
        if (row >= cumulative_rows_[i]) {
            return {i, row - cumulative_rows_[i]};
        }
    }
    return {0, row};
}

Table& ConcatTable::table_at(std::size_t index) {
    return *tables_.at(index);
}

const Table& ConcatTable::table_at(std::size_t index) const {
    return *tables_.at(index);
}

} // namespace casacore_mini
