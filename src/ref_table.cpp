#include "casacore_mini/ref_table.hpp"

#include <stdexcept>

namespace casacore_mini {

RefTable::RefTable(Table& base, std::vector<std::uint64_t> row_indices)
    : base_(&base), rows_(std::move(row_indices)) {}

RefTable::RefTable(Table& base, const std::vector<bool>& row_mask) : base_(&base) {
    for (std::uint64_t i = 0; i < row_mask.size(); ++i) {
        if (row_mask[i]) rows_.push_back(i);
    }
}

RefTable::RefTable(Table& base) : base_(&base) {}

std::uint64_t RefTable::nrow() const {
    return rows_.size();
}

std::size_t RefTable::ncolumn() const {
    return base_->ncolumn();
}

const std::vector<ColumnDesc>& RefTable::columns() const {
    return base_->columns();
}

const Record& RefTable::keywords() const {
    return base_->keywords();
}

std::string RefTable::table_name() const {
    return base_->table_name();
}

CellValue RefTable::read_scalar_cell(std::string_view col_name, std::uint64_t row) const {
    return base_->read_scalar_cell(col_name, base_row(row));
}

std::vector<double> RefTable::read_array_double_cell(std::string_view col_name,
                                                      std::uint64_t row) const {
    return base_->read_array_double_cell(col_name, base_row(row));
}

std::vector<float> RefTable::read_array_float_cell(std::string_view col_name,
                                                    std::uint64_t row) const {
    return base_->read_array_float_cell(col_name, base_row(row));
}

std::vector<std::int32_t> RefTable::read_array_int_cell(std::string_view col_name,
                                                         std::uint64_t row) const {
    return base_->read_array_int_cell(col_name, base_row(row));
}

const std::vector<std::uint64_t>& RefTable::row_indices() const {
    return rows_;
}

std::uint64_t RefTable::base_row(std::uint64_t view_row) const {
    if (view_row >= rows_.size()) {
        throw std::out_of_range("RefTable::base_row: row " + std::to_string(view_row) +
                                " out of range (nrow=" + std::to_string(rows_.size()) + ")");
    }
    return rows_[view_row];
}

void RefTable::add_row(std::uint64_t base_row_index) {
    rows_.push_back(base_row_index);
}

Table& RefTable::base_table() {
    return *base_;
}

const Table& RefTable::base_table() const {
    return *base_;
}

} // namespace casacore_mini
