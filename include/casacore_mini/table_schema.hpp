#pragma once

#include "casacore_mini/platform.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// Storage manager binding metadata for a table or column.
struct StorageManagerInfo {
    std::string manager_class;
    std::string file;
    std::string name;

    [[nodiscard]] bool operator==(const StorageManagerInfo& other) const = default;
};

/// Parsed schema information for a single column.
struct ColumnSchema {
    std::string name;
    std::string data_type;
    std::string descriptor;
    std::string value_kind;
    std::optional<std::vector<std::size_t>> shape;
    std::optional<std::size_t> ndim;
    std::optional<StorageManagerInfo> storage_manager;

    [[nodiscard]] bool operator==(const ColumnSchema& other) const = default;
};

/// Parsed schema summary for one table.
struct TableSchema {
    std::string table_path;
    std::string table_kind;
    std::size_t row_count = 0;
    std::size_t column_count = 0;
    std::vector<StorageManagerInfo> storage_managers;
    std::vector<ColumnSchema> columns;

    [[nodiscard]] bool operator==(const TableSchema& other) const = default;
};

/// Parse schema-oriented sections from `showtableinfo` textual output.
///
/// This parser is a transitional interface used by the Phase 1 schema hook.
[[nodiscard]] TableSchema parse_showtableinfo_schema(std::string_view showtableinfo_text);

} // namespace casacore_mini
