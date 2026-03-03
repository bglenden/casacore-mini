// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Table measure descriptor: MEASINFO keyword codec and QuantumUnits.

///
/// Descriptor for a measure-bearing table column, extracted from MEASINFO keywords.
///
///
///
///
///
/// casacore attaches a `MEASINFO` sub-record and an optional `QuantumUnits`
/// entry to the keyword set of any column that holds a physical measure.
/// `TableMeasDesc` mirrors these keyword fields in a structured form that is
/// independent of the underlying keyword representation (text or binary).
///
/// Fields:
/// - `measure_type` — the kind of measure (epoch, direction, uvw, position, …).
/// - `default_ref` — the default reference frame when no per-row column is used.
/// - `var_ref_column` — name of a companion scalar column that stores per-row
///   reference codes (empty when the reference is fixed).
/// - `tab_ref_types` / `tab_ref_codes` — the reference name/code mapping for
///   variable-reference columns, parallel arrays.
/// - `units` — physical units for the numeric values in the column.
/// - `offset` / `offset_column` / `offset_per_array` — optional measure offset
///   applied during reconstruction.
///
/// The codec functions `read_table_measure_desc` and `write_table_measure_desc`
/// translate between this struct and a binary `Record` column keyword set.
///
///
/// @par Example
/// @code{.cpp}
///   auto desc_opt = read_table_measure_desc("TIME", column_keywords);
///   if (desc_opt) {
///       const auto& desc = *desc_opt;
///       std::cout << "units: " << desc.units[0] << "\n";  // "s"
///   }
/// @endcode
///
struct TableMeasDesc {
    /// Column name that holds the measure data values.
    std::string column_name;
    /// Measure type (epoch, direction, position, ...).
    MeasureType measure_type{};
    /// Default reference frame when no variable-reference column is used.
    MeasureRefType default_ref;
    /// Per-row reference column name (empty if fixed reference).
    std::string var_ref_column;
    /// Allowed reference type names for variable-reference columns.
    std::vector<std::string> tab_ref_types;
    /// Numeric codes for variable-reference columns, parallel to tab_ref_types.
    std::vector<std::uint32_t> tab_ref_codes;
    /// Physical units for the column values (e.g. {"s"}, {"rad","rad"}, {"m","m","m"}).
    std::vector<std::string> units;
    /// Optional fixed offset measure (encoded as a MeasureHolder record).
    std::optional<Measure> offset;
    /// Variable offset column name (empty if no variable offset).
    std::string offset_column;
    /// True when the variable offset applies per array element (not per row).
    bool offset_per_array = false;

    /// True when reference codes vary per row.
    [[nodiscard]] bool has_variable_ref() const noexcept {
        return !var_ref_column.empty();
    }
    /// True when an offset is present (fixed or variable).
    [[nodiscard]] bool has_offset() const noexcept {
        return offset.has_value() || !offset_column.empty();
    }
};

/// Read a TableMeasDesc from a column's keyword set (binary Record).
///
/// Looks for a "MEASINFO" sub-record and optional "QuantumUnits" or
/// "VariableUnits" entries in the column keywords.
///
/// @param column_name  The column name (for error messages and descriptor).
/// @param column_keywords  The column's binary keyword Record.
/// @return  The parsed descriptor, or std::nullopt if no MEASINFO is present.
[[nodiscard]] std::optional<TableMeasDesc> read_table_measure_desc(const std::string& column_name,
                                                                   const Record& column_keywords);

/// Write a TableMeasDesc back into a column keyword Record.
///
/// Creates/replaces the "MEASINFO" sub-record and "QuantumUnits" entries.
void write_table_measure_desc(const TableMeasDesc& desc, Record& column_keywords);

///
/// Descriptor for quantum units on a column (without full measure semantics).
///
///
///
///
///
/// Some columns carry physical units but are not full measures.  For example,
/// a `WEIGHT` column may have units of `"Jy^{-2}"` without being a Measure.
/// `TableQuantumDesc` captures the `QuantumUnits` keyword entry for such
/// columns.
///
/// When `var_units_column` is non-empty, the units vary per row and are read
/// from the named companion column rather than from the fixed `units` vector.
///
struct TableQuantumDesc {
    /// Column name.
    std::string column_name;
    /// Fixed unit strings (e.g. {"s"}, {"Jy"}).
    std::vector<std::string> units;
    /// Variable units column name (empty if fixed).
    std::string var_units_column;

    [[nodiscard]] bool has_variable_units() const noexcept {
        return !var_units_column.empty();
    }
};

/// Read a TableQuantumDesc from column keywords.
[[nodiscard]] std::optional<TableQuantumDesc>
read_table_quantum_desc(const std::string& column_name, const Record& column_keywords);

/// Write a TableQuantumDesc into column keywords.
void write_table_quantum_desc(const TableQuantumDesc& desc, Record& column_keywords);

} // namespace casacore_mini
