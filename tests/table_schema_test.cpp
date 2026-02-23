#include "casacore_mini/table_schema.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool expect_true(const bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

} // namespace

int main() noexcept {
    try {
        const std::filesystem::path fixture_path =
            std::filesystem::path(CASACORE_MINI_SOURCE_DIR) /
            "data/corpus/fixtures/replay_ttableaccess/showtableinfo.txt";
        const auto schema = casacore_mini::parse_showtableinfo_schema(read_file(fixture_path));

        if (!expect_true(schema.row_count == 2U, "row_count mismatch")) {
            return 1;
        }
        if (!expect_true(schema.column_count == 2U, "column_count mismatch")) {
            return 1;
        }
        if (!expect_true(schema.table_kind == "table", "table_kind mismatch")) {
            return 1;
        }
        if (!expect_true(schema.storage_managers.size() == 1U, "storage manager count mismatch")) {
            return 1;
        }

        const auto& manager = schema.storage_managers.front();
        if (!expect_true(manager.manager_class == "IncrementalStMan", "manager class mismatch")) {
            return 1;
        }
        if (!expect_true(manager.file == "table.f0", "manager file mismatch")) {
            return 1;
        }

        if (!expect_true(schema.columns.size() == 2U, "column count mismatch")) {
            return 1;
        }

        const auto& arrcol = schema.columns[0];
        if (!expect_true(arrcol.name == "arrcol", "arrcol.name mismatch")) {
            return 1;
        }
        if (!expect_true(arrcol.data_type == "Int", "arrcol.data_type mismatch")) {
            return 1;
        }
        if (!expect_true(arrcol.value_kind == "array", "arrcol.value_kind mismatch")) {
            return 1;
        }
        if (!expect_true(arrcol.storage_manager.has_value(), "arrcol manager missing")) {
            return 1;
        }

        const auto& scacol = schema.columns[1];
        if (!expect_true(scacol.name == "scacol", "scacol.name mismatch")) {
            return 1;
        }
        if (!expect_true(scacol.data_type == "Int", "scacol.data_type mismatch")) {
            return 1;
        }
        if (!expect_true(scacol.value_kind == "scalar", "scacol.value_kind mismatch")) {
            return 1;
        }
        if (!expect_true(scacol.storage_manager.has_value(), "scacol manager missing")) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_schema_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
