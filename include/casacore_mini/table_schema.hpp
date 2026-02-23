#pragma once

#include "casacore_mini/platform.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Transitional parser for schema metadata from `showtableinfo` text.

/// Storage manager binding metadata for a table or column.
struct StorageManagerInfo {
    /// Storage manager class name (for example `StandardStMan`).
    std::string manager_class;
    /// Backing file token/path shown by `showtableinfo`.
    std::string file;
    /// Storage manager instance name.
    std::string name;

    [[nodiscard]] bool operator==(const StorageManagerInfo& other) const = default;
};

/// Parsed schema information for a single column.
struct ColumnSchema {
    /// Column name.
    std::string name;
    /// Column data type token.
    std::string data_type;
    /// Descriptor token line from `showtableinfo`.
    std::string descriptor;
    /// Value kind (`Scalar`, `Array`, etc.).
    std::string value_kind;
    /// Optional fixed shape for array-like columns.
    std::optional<std::vector<std::size_t>> shape;
    /// Optional rank (`ndim`) metadata.
    std::optional<std::size_t> ndim;
    /// Optional storage-manager binding for the column.
    std::optional<StorageManagerInfo> storage_manager;

    [[nodiscard]] bool operator==(const ColumnSchema& other) const = default;
};

/// Parsed schema summary for one table.
struct TableSchema {
    /// Reported table path.
    std::string table_path;
    /// Reported table kind token.
    std::string table_kind;
    /// Number of rows reported in schema output.
    std::size_t row_count = 0;
    /// Number of columns reported in schema output.
    std::size_t column_count = 0;
    /// Table-level storage manager declarations.
    std::vector<StorageManagerInfo> storage_managers;
    /// Column schema entries in report order.
    std::vector<ColumnSchema> columns;

    [[nodiscard]] bool operator==(const TableSchema& other) const = default;
};

/// Parse schema-oriented sections from `showtableinfo` textual output.
///
/// This parser is a transitional interface used by the Phase 1 schema hook.
///
/// @throws std::runtime_error if required schema sections are missing or malformed.
[[nodiscard]] TableSchema parse_showtableinfo_schema(std::string_view showtableinfo_text);

} // namespace casacore_mini
