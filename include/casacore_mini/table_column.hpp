// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/table.hpp"

#include <cassert>
#include <complex>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Typed column access templates: ScalarColumn<T> and ArrayColumn<T>.
///
/// These provide column-oriented typed access, hiding SM details.
/// casacore-original equivalent: ScalarColumn<Int>, ArrayColumn<Float>.

/// 
/// Typed accessor for a scalar column in a `Table`.
/// 
///
///
///
/// 
/// `ScalarColumn<T>` provides a convenient, type-safe interface for reading
/// and writing individual scalar cell values in a named column.  It validates
/// at construction time that the named column exists in the table, then
/// delegates each `get` and `put` call to the appropriate `Table` storage-
/// manager path.
///
/// The template parameter `T` must match the column's declared data type.
/// A type mismatch at `get` time raises `std::runtime_error`.
/// 
///
/// @par Example
/// @code{.cpp}
///   Table t = Table::open("my_vis.ms");
///   ScalarColumn<int32_t> field_id(t, "FIELD_ID");
///   for (uint64_t row = 0; row < t.nrow(); ++row) {
///       std::cout << row << " -> " << field_id.get(row) << "\n";
///   }
///   field_id.put(0, 42);
/// @endcode
/// 
template <typename T> class ScalarColumn {
  public:
    ScalarColumn(Table& table, std::string_view name) : table_(&table), name_(name) {
        // Verify column exists.
        if (table_->find_column_desc(name_) == nullptr) {
            throw std::runtime_error("ScalarColumn: column '" + name_ + "' not found");
        }
    }

    /// Read the value at the given row.
    [[nodiscard]] T get(std::uint64_t row) const {
        auto cell = table_->read_scalar_cell(name_, row);
        if (auto* p = std::get_if<T>(&cell)) {
            return *p;
        }
        throw std::runtime_error("ScalarColumn::get: type mismatch for column '" + name_ + "'");
    }

    /// Write a value at the given row.
    void put(std::uint64_t row, const T& value) {
        table_->write_scalar_cell(name_, row, CellValue(value));
    }

    /// Number of rows.
    [[nodiscard]] std::uint64_t nrow() const {
        return table_->nrow();
    }

  private:
    Table* table_;
    std::string name_;
};

/// 
/// Typed accessor for an array column in a `Table`.
/// 
///
///
///
/// 
/// `ArrayColumn<T>` provides type-safe read and write access to fixed-shape or
/// variable-shape array cells in a named column.  It validates column existence
/// at construction and delegates each `get` and `put` call to the appropriate
/// `Table` storage-manager specialization.
///
/// Supported element types are `float`, `double`, `int32_t`, and `uint8_t`
/// (raw bytes), each backed by an explicit template specialization that routes
/// through the corresponding `Table::read_array_*_cell` method.
///
/// `shape()` returns the column's declared fixed shape from the column
/// descriptor; for variable-shape columns this value may not reflect the
/// actual shape of a specific row.
/// 
///
/// @par Example
/// @code{.cpp}
///   Table t = Table::open("my_vis.ms");
///   ArrayColumn<double> uvw(t, "UVW");
///   std::vector<double> vals = uvw.get(0);   // [u, v, w] for row 0
///   uvw.put(0, {1.0, 2.0, 3.0});
/// @endcode
/// 
template <typename T> class ArrayColumn {
  public:
    ArrayColumn(Table& table, std::string_view name) : table_(&table), name_(name) {
        if (table_->find_column_desc(name_) == nullptr) {
            throw std::runtime_error("ArrayColumn: column '" + name_ + "' not found");
        }
    }

    /// Read all elements of the array at the given row.
    [[nodiscard]] std::vector<T> get(std::uint64_t row) const;

    /// Write array elements at the given row.
    void put(std::uint64_t row, const std::vector<T>& values);

    /// Get the shape of the array at the given row (from column descriptor).
    [[nodiscard]] std::vector<std::int64_t> shape() const {
        const auto* cd = table_->find_column_desc(name_);
        assert(cd != nullptr);
        return cd->shape;
    }

    /// Number of rows.
    [[nodiscard]] std::uint64_t nrow() const {
        return table_->nrow();
    }

  private:
    Table* table_;
    std::string name_;
};

// -- Template specializations for get/put --

template <> inline std::vector<float> ArrayColumn<float>::get(std::uint64_t row) const {
    return table_->read_array_float_cell(name_, row);
}

template <> inline std::vector<double> ArrayColumn<double>::get(std::uint64_t row) const {
    return table_->read_array_double_cell(name_, row);
}

template <>
inline void ArrayColumn<float>::put(std::uint64_t row, const std::vector<float>& values) {
    table_->write_array_float_cell(name_, row, values);
}

template <>
inline void ArrayColumn<double>::put(std::uint64_t row, const std::vector<double>& values) {
    table_->write_array_double_cell(name_, row, values);
}

// -- int32_t array specializations --

template <>
inline std::vector<std::int32_t> ArrayColumn<std::int32_t>::get(std::uint64_t row) const {
    return table_->read_array_int_cell(name_, row);
}

template <>
inline void ArrayColumn<std::int32_t>::put(std::uint64_t row,
                                           const std::vector<std::int32_t>& values) {
    table_->write_array_int_cell(name_, row, values);
}

// -- raw uint8_t array specializations --

template <>
inline std::vector<std::uint8_t> ArrayColumn<std::uint8_t>::get(std::uint64_t row) const {
    return table_->read_array_raw_cell(name_, row);
}

template <>
inline void ArrayColumn<std::uint8_t>::put(std::uint64_t row,
                                           const std::vector<std::uint8_t>& values) {
    table_->write_array_raw_cell(name_, row, values);
}

} // namespace casacore_mini
