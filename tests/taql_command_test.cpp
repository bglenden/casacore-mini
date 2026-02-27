/// @file taql_command_test.cpp
/// @brief Tests for TaQL command execution against real tables.

#include "casacore_mini/taql.hpp"
#include "casacore_mini/table.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace casacore_mini;
namespace fs = std::filesystem;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "FAIL: " << label << "\n";
    }
}

/// Create a small test table with 10 rows and 3 columns.
static fs::path create_test_table() {
    auto path = fs::temp_directory_path() / "taql_cmd_test_table";
    if (fs::exists(path)) fs::remove_all(path);

    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, "Row ID"},
        {"VALUE", DataType::tp_double, ColumnKind::scalar, {}, "A value"},
        {"NAME", DataType::tp_string, ColumnKind::scalar, {}, "A name"},
    };
    auto table = Table::create(path, cols, 10);
    for (std::uint64_t r = 0; r < 10; ++r) {
        table.write_scalar_cell("ID", r, CellValue(static_cast<std::int32_t>(r)));
        table.write_scalar_cell("VALUE", r, CellValue(static_cast<double>(r) * 1.5));
        table.write_scalar_cell("NAME", r, CellValue(std::string("item_") + std::to_string(r)));
    }
    table.flush();
    return path;
}

// ---------------------------------------------------------------------------
// SELECT tests
// ---------------------------------------------------------------------------

static bool test_select_all() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SELECT * FROM t", table);
    check(result.rows.size() == 10, "select all returns 10 rows");
    check(!result.column_names.empty(), "select * has column names");
    fs::remove_all(path);
    return g_fail == 0;
}

static bool test_select_where() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SELECT ID FROM t WHERE VALUE > 6.0", table);
    // VALUE > 6.0 means rows where r*1.5 > 6.0, so r > 4, rows 5-9 = 5 rows
    check(result.rows.size() == 5, "select where returns 5 rows");
    // Check first matching row is 5
    check(result.rows[0] == 5, "first matching row is 5");
    fs::remove_all(path);
    return g_fail == 0;
}

static bool test_select_orderby() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SELECT ID FROM t ORDERBY VALUE DESC", table);
    check(result.rows.size() == 10, "orderby returns all rows");
    check(result.rows[0] == 9, "first row is 9 (desc order)");
    check(result.rows[9] == 0, "last row is 0 (desc order)");
    fs::remove_all(path);
    return g_fail == 0;
}

static bool test_select_limit_offset() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SELECT ID FROM t LIMIT 3 OFFSET 2", table);
    check(result.rows.size() == 3, "limit 3 returns 3 rows");
    check(result.rows[0] == 2, "offset 2 starts at row 2");
    fs::remove_all(path);
    return g_fail == 0;
}

static bool test_select_projection_values() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SELECT ID FROM t WHERE ID = 3", table);
    check(result.rows.size() == 1, "single row match");
    check(result.values.size() == 1, "1 projection value");
    auto id_val = std::get_if<std::int64_t>(&result.values[0]);
    check(id_val != nullptr && *id_val == 3, "projected ID=3");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// COUNT
// ---------------------------------------------------------------------------

static bool test_count() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("COUNT FROM t WHERE VALUE > 6.0", table);
    check(result.affected_rows == 5, "count=5");
    check(!result.values.empty(), "count has value");
    check(std::get<std::int64_t>(result.values[0]) == 5, "count value=5");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// CALC with table
// ---------------------------------------------------------------------------

static bool test_calc_with_table() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("CALC 2 + 3", table);
    check(result.values.size() == 1, "calc with table returns 1 value");
    check(std::get<std::int64_t>(result.values[0]) == 5, "calc 2+3=5");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// UPDATE
// ---------------------------------------------------------------------------

static bool test_update() {
    auto path = create_test_table();
    auto table = Table::open_rw(path);
    auto result = taql_execute("UPDATE t SET VALUE = 99.0 WHERE ID = 5", table);
    check(result.affected_rows == 1, "update affected 1 row");

    // Verify the update
    auto cv = table.read_scalar_cell("VALUE", 5);
    auto* dv = std::get_if<double>(&cv);
    check(dv != nullptr && *dv == 99.0, "value updated to 99.0");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// DELETE
// ---------------------------------------------------------------------------

static bool test_delete() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("DELETE FROM t WHERE ID > 7", table);
    check(result.affected_rows == 2, "delete marks 2 rows");
    check(result.rows.size() == 2, "delete returns 2 row indices");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// SHOW
// ---------------------------------------------------------------------------

static bool test_show() {
    auto path = create_test_table();
    auto table = Table::open(path);
    auto result = taql_execute("SHOW functions", table);
    check(!result.show_text.empty(), "show returns text");
    check(result.show_text.find("sin") != std::string::npos, "show mentions sin");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Expression with column references
// ---------------------------------------------------------------------------

static bool test_expr_with_columns() {
    auto path = create_test_table();
    auto table = Table::open(path);
    // VALUE column = row * 1.5, so VALUE + ID should be row*1.5 + row = row*2.5
    auto result = taql_execute("SELECT VALUE FROM t WHERE VALUE + 1 > 10", table);
    // VALUE > 9 means row*1.5 > 9, so row > 6, rows 7-9 = 3 rows
    check(result.rows.size() == 3, "expr with columns: 3 rows");
    fs::remove_all(path);
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_select_all();
    test_select_where();
    test_select_orderby();
    test_select_limit_offset();
    test_select_projection_values();
    test_count();
    test_calc_with_table();
    test_update();
    test_delete();
    test_show();
    test_expr_with_columns();

    std::cout << "taql_command_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
