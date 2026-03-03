// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
namespace fs = std::filesystem;
using namespace casacore_mini;

int main() {
    auto dir = fs::temp_directory_path() / "casacore_mini_drop_debug";
    fs::create_directories(dir);
    std::vector<TableColumnSpec> cols = {{"X", DataType::tp_int, ColumnKind::scalar, {}, ""}};
    auto table = Table::create(dir, cols, 1);
    table.flush();

    std::printf("Path: %s\n", dir.string().c_str());
    std::printf("Exists before: %d\n", fs::exists(dir) ? 1 : 0);

    auto ast = taql_parse("DROP TABLE '" + dir.string() + "'");
    std::printf("Command enum: %d (expected %d)\n", static_cast<int>(ast.command),
                static_cast<int>(TaqlCommand::drop_table_cmd));
    std::printf("drop_tables.size: %zu\n", ast.drop_tables.size());
    for (const auto& dt : ast.drop_tables) {
        std::printf("  entry: '%s'\n", dt.c_str());
    }

    (void)taql_calc("DROP TABLE '" + dir.string() + "'");
    std::printf("Exists after: %d\n", fs::exists(dir) ? 1 : 0);

    // Cleanup just in case
    std::error_code ec;
    fs::remove_all(dir, ec);
    return 0;
}
