#include "casacore_mini/table_iterator.hpp"

#include <algorithm>
#include <stdexcept>

namespace casacore_mini {

namespace {

/// Compare two CellValues for ordering. Returns -1, 0, or 1.
int cell_compare(const CellValue& a, const CellValue& b) {
    if (a.index() != b.index()) {
        return a.index() < b.index() ? -1 : 1;
    }
    return std::visit(
        [&b](const auto& va) -> int {
            using T = std::decay_t<decltype(va)>;
            const auto& vb = std::get<T>(b);
            if constexpr (std::is_same_v<T, std::complex<float>> ||
                          std::is_same_v<T, std::complex<double>>) {
                // Complex: compare by real then imag.
                if (va.real() < vb.real()) return -1;
                if (va.real() > vb.real()) return 1;
                if (va.imag() < vb.imag()) return -1;
                if (va.imag() > vb.imag()) return 1;
                return 0;
            } else {
                if (va < vb) return -1;
                if (vb < va) return 1;
                return 0;
            }
        },
        a);
}

} // namespace

TableIterator::TableIterator(Table& table, std::vector<std::string> key_columns)
    : table_(&table), key_columns_(std::move(key_columns)) {
    if (key_columns_.empty()) {
        throw std::runtime_error("TableIterator: must have at least one key column");
    }
    build_sorted_rows();
}

void TableIterator::build_sorted_rows() {
    auto n = table_->nrow();
    sorted_rows_.resize(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        sorted_rows_[i] = i;
    }

    // Sort rows by key column values.
    std::sort(sorted_rows_.begin(), sorted_rows_.end(),
              [this](std::uint64_t a, std::uint64_t b) {
                  for (const auto& col : key_columns_) {
                      auto va = table_->read_scalar_cell(col, a);
                      auto vb = table_->read_scalar_cell(col, b);
                      int cmp = cell_compare(va, vb);
                      if (cmp < 0) return true;
                      if (cmp > 0) return false;
                  }
                  return false;
              });
}

bool TableIterator::keys_equal(std::uint64_t row_a, std::uint64_t row_b) const {
    for (const auto& col : key_columns_) {
        auto va = table_->read_scalar_cell(col, row_a);
        auto vb = table_->read_scalar_cell(col, row_b);
        if (cell_compare(va, vb) != 0) return false;
    }
    return true;
}

bool TableIterator::next() {
    if (sorted_rows_.empty()) return false;

    if (!started_) {
        started_ = true;
        group_start_ = 0;
    } else {
        group_start_ = group_end_;
    }

    if (group_start_ >= sorted_rows_.size()) return false;

    // Find end of current group.
    group_end_ = group_start_ + 1;
    while (group_end_ < sorted_rows_.size() &&
           keys_equal(sorted_rows_[group_start_], sorted_rows_[group_end_])) {
        ++group_end_;
    }

    // Build RefTable for this group.
    std::vector<std::uint64_t> group_rows(sorted_rows_.begin() + static_cast<std::ptrdiff_t>(group_start_),
                                           sorted_rows_.begin() + static_cast<std::ptrdiff_t>(group_end_));
    current_group_.emplace(*table_, std::move(group_rows));
    return true;
}

void TableIterator::reset() {
    started_ = false;
    group_start_ = 0;
    group_end_ = 0;
    current_group_.reset();
}

const RefTable& TableIterator::current() const {
    if (!current_group_) {
        throw std::runtime_error("TableIterator::current: no current group (call next() first)");
    }
    return *current_group_;
}

RefTable& TableIterator::current() {
    if (!current_group_) {
        throw std::runtime_error("TableIterator::current: no current group (call next() first)");
    }
    return *current_group_;
}

bool TableIterator::at_end() const {
    return started_ && group_start_ >= sorted_rows_.size();
}

} // namespace casacore_mini
