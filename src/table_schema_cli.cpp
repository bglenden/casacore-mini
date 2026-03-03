// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_schema.hpp"

#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string encode_shape(const std::optional<std::vector<std::size_t>>& shape) {
    if (!shape.has_value()) {
        return "-";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < shape->size(); ++index) {
        if (index > 0) {
            output << ',';
        }
        output << shape->at(index);
    }
    return output.str();
}

[[nodiscard]] std::string encode_ndim(const std::optional<std::size_t>& ndim) {
    if (!ndim.has_value()) {
        return "-";
    }

    return std::to_string(*ndim);
}

void print_column_storage_manager(const std::optional<casacore_mini::StorageManagerInfo>& manager) {
    if (!manager.has_value()) {
        std::cout << "-|-|-";
        return;
    }

    std::cout << manager->manager_class << '|' << manager->file << '|' << manager->name;
}

} // namespace

int main(int argc, char** argv) noexcept {
    if (argc != 2) {
        std::cerr << "Usage: table_schema_cli <showtableinfo_text_file>\n";
        return 2;
    }

    try {
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) {
            std::cerr << "Cannot open input file: " << argv[1] << '\n';
            return 2;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        const auto schema = casacore_mini::parse_showtableinfo_schema(buffer.str());

        std::cout << "table_path=" << schema.table_path << '\n';
        std::cout << "table_kind=" << schema.table_kind << '\n';
        std::cout << "row_count=" << schema.row_count << '\n';
        std::cout << "column_count=" << schema.column_count << '\n';

        for (const auto& manager : schema.storage_managers) {
            std::cout << "storage_manager=" << manager.manager_class << '|' << manager.file << '|'
                      << manager.name << '\n';
        }

        for (const auto& column : schema.columns) {
            std::cout << "column=" << column.name << '|' << column.data_type << '|'
                      << column.value_kind << '|' << encode_ndim(column.ndim) << '|'
                      << encode_shape(column.shape) << '|';
            print_column_storage_manager(column.storage_manager);
            std::cout << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "table_schema_cli failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
