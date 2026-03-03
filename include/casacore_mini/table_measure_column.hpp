// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_measure_desc.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Column adapters that produce Measure objects from table rows.

/// Callback type for reading a scalar cell value by column name and row.
using ScalarCellReader = std::function<CellValue(const std::string& col_name, std::uint64_t row)>;

/// Callback type for reading an array cell as flat double vector by column name and row.
/// Returns the values in Fortran order (first axis varying fastest).
using ArrayCellReader =
    std::function<std::vector<double>(const std::string& col_name, std::uint64_t row)>;

/// Callback type for writing a scalar cell value by column index and row.
using ScalarCellWriter =
    std::function<void(std::size_t col_index, std::uint64_t row, const CellValue& value)>;

/// Read a scalar measure from a table row.
///
/// <synopsis>
/// Uses the `TableMeasDesc` descriptor to determine the measure type,
/// reference frame, and physical units, then reads the raw numeric data
/// through the `read_scalar` callback.
///
/// For columns with a fixed reference frame, the frame is taken directly from
/// `desc.default_ref`.  For variable-reference columns, the reference code is
/// read from the column named by `desc.var_ref_column` and mapped to a
/// `MeasureRefType` via `desc.tab_ref_codes`/`desc.tab_ref_types`.
/// </synopsis>
///
/// @param desc  The measure descriptor for this column.
/// @param read_scalar  Callback to read scalar cell values.
/// @param row  The row number to read.
/// @return  The reconstructed Measure.
/// @throws std::invalid_argument on type mismatch or missing data.
[[nodiscard]] Measure read_scalar_measure(const TableMeasDesc& desc,
                                          const ScalarCellReader& read_scalar, std::uint64_t row);

/// Read an array of measures from a table row.
///
/// <synopsis>
/// For array measure columns (e.g. UVW with shape [3, nbaseline]), reads the
/// full flat double array and splits it into individual `Measure` objects.
/// Each measure's value components correspond to one slice along the leading
/// axis.
/// </synopsis>
///
/// @param desc  The measure descriptor for this column.
/// @param read_array  Callback to read array cell values as flat doubles.
/// @param read_scalar  Callback to read scalar reference columns (for variable refs).
/// @param row  The row number to read.
/// @return  Vector of measures extracted from the array.
[[nodiscard]] std::vector<Measure> read_array_measures(const TableMeasDesc& desc,
                                                       const ArrayCellReader& read_array,
                                                       const ScalarCellReader& read_scalar,
                                                       std::uint64_t row);

/// Write a scalar measure into a table row.
///
/// <synopsis>
/// Writes the numeric value components via the `write_scalar` callback.
/// Does NOT write the `MEASINFO` keywords; use `write_table_measure_desc`
/// on the column keyword record for that.
/// </synopsis>
///
/// @param desc  The measure descriptor for this column.
/// @param write_scalar  Callback to write scalar cell values.
/// @param col_index  Column index for the data column.
/// @param row  Row number.
/// @param m  The measure to write.
void write_scalar_measure(const TableMeasDesc& desc, const ScalarCellWriter& write_scalar,
                          std::size_t col_index, std::uint64_t row, const Measure& m);

/// Write an array of measures into a table row.
///
/// <synopsis>
/// Packs multiple measures into a flat double array and writes via callback.
/// The packing layout is the inverse of `read_array_measures`.
/// </synopsis>
///
/// @param desc  The measure descriptor for this column.
/// @param write_fn  Callback: receives column name, row, and flat double vector.
/// @param row  Row number.
/// @param measures  The measures to pack and write.
void write_array_measures(const TableMeasDesc& desc,
                          const std::function<void(const std::string& col_name, std::uint64_t row,
                                                   const std::vector<double>& values)>& write_fn,
                          std::uint64_t row, const std::vector<Measure>& measures);

} // namespace casacore_mini
