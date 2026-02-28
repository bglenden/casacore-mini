#include "casacore_mini/taql.hpp"
#include "casacore_mini/table.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

static int check_count = 0;

static void check(bool condition, const char* msg) {
    ++check_count;
    if (!condition) {
        std::cerr << "FAIL: " << msg << "\n";
        std::abort();
    }
}

static double as_dbl(const TaqlValue& v) {
    if (auto* d = std::get_if<double>(&v)) return *d;
    if (auto* i = std::get_if<std::int64_t>(&v)) return static_cast<double>(*i);
    throw std::runtime_error("not a number");
}

static std::int64_t as_i64(const TaqlValue& v) {
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i;
    throw std::runtime_error("not an int64");
}

// Helper to create a test table
static std::pair<fs::path, Table> make_test_table() {
    auto tmp = fs::temp_directory_path() / "taql_dml_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"id", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"val", DataType::tp_double, ColumnKind::scalar, {}, ""},
        {"name", DataType::tp_string, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 3);
    table.write_scalar_cell("id", 0, std::int32_t{1});
    table.write_scalar_cell("id", 1, std::int32_t{2});
    table.write_scalar_cell("id", 2, std::int32_t{3});
    table.write_scalar_cell("val", 0, 10.0);
    table.write_scalar_cell("val", 1, 20.0);
    table.write_scalar_cell("val", 2, 30.0);
    table.write_scalar_cell("name", 0, std::string("alice"));
    table.write_scalar_cell("name", 1, std::string("bob"));
    table.write_scalar_cell("name", 2, std::string("carol"));
    table.flush();
    return {tmp, std::move(table)};
}

// ---- INSERT VALUES ----
static void test_insert_values() {
    auto [tmp, table] = make_test_table();

    check(table.nrow() == 3, "insert: initial 3 rows");

    auto r = taql_execute("INSERT INTO t (id, val, name) VALUES (4, 40.0, 'dave')", table);
    check(r.affected_rows == 1, "insert: 1 row affected");
    check(table.nrow() == 4, "insert: now 4 rows");

    // Verify inserted data
    auto cv_id = table.read_scalar_cell("id", 3);
    auto cv_val = table.read_scalar_cell("val", 3);
    auto cv_name = table.read_scalar_cell("name", 3);

    if (auto* i = std::get_if<std::int32_t>(&cv_id))
        check(*i == 4, "insert: id=4");
    if (auto* d = std::get_if<double>(&cv_val))
        check(std::abs(*d - 40.0) < 1e-10, "insert: val=40.0");
    if (auto* s = std::get_if<std::string>(&cv_name))
        check(*s == "dave", "insert: name=dave");

    fs::remove_all(tmp);
}

// ---- INSERT multiple rows ----
static void test_insert_multiple_rows() {
    auto [tmp, table] = make_test_table();

    auto r = taql_execute("INSERT INTO t (id, val) VALUES (10, 100.0), (20, 200.0)", table);
    check(r.affected_rows == 2, "insert multi: 2 rows affected");
    check(table.nrow() == 5, "insert multi: now 5 rows");

    auto cv = table.read_scalar_cell("val", 4);
    if (auto* d = std::get_if<double>(&cv))
        check(std::abs(*d - 200.0) < 1e-10, "insert multi: second row val");

    fs::remove_all(tmp);
}

// ---- DELETE with WHERE ----
static void test_delete_where() {
    auto [tmp, table] = make_test_table();

    check(table.nrow() == 3, "delete: initial 3 rows");

    auto r = taql_execute("DELETE FROM t WHERE id == 2", table);
    check(r.affected_rows == 1, "delete: 1 row affected");
    check(table.nrow() == 2, "delete: now 2 rows");

    // Remaining rows should be id=1 and id=3
    auto cv0 = table.read_scalar_cell("id", 0);
    auto cv1 = table.read_scalar_cell("id", 1);
    if (auto* i = std::get_if<std::int32_t>(&cv0))
        check(*i == 1, "delete: row 0 is id=1");
    if (auto* i = std::get_if<std::int32_t>(&cv1))
        check(*i == 3, "delete: row 1 is id=3");

    fs::remove_all(tmp);
}

// ---- DELETE all rows (no WHERE) ----
static void test_delete_all() {
    auto [tmp, table] = make_test_table();

    auto r = taql_execute("DELETE FROM t", table);
    check(r.affected_rows == 3, "delete all: 3 rows affected");
    check(table.nrow() == 0, "delete all: 0 rows remain");

    fs::remove_all(tmp);
}

// ---- DELETE with LIMIT ----
static void test_delete_limit() {
    auto [tmp, table] = make_test_table();

    auto r = taql_execute("DELETE FROM t LIMIT 2", table);
    check(r.affected_rows == 2, "delete limit: 2 rows affected");
    check(table.nrow() == 1, "delete limit: 1 row remains");

    // The remaining row should be the last one (id=3)
    auto cv = table.read_scalar_cell("id", 0);
    if (auto* i = std::get_if<std::int32_t>(&cv))
        check(*i == 3, "delete limit: remaining is id=3");

    fs::remove_all(tmp);
}

// ---- UPDATE (existing, verify it still works) ----
static void test_update() {
    auto [tmp, table] = make_test_table();

    auto r = taql_execute("UPDATE t SET val = 99.0 WHERE id == 2", table);
    check(r.affected_rows == 1, "update: 1 row affected");

    auto cv = table.read_scalar_cell("val", 1);
    if (auto* d = std::get_if<double>(&cv))
        check(std::abs(*d - 99.0) < 1e-10, "update: val updated to 99");

    fs::remove_all(tmp);
}

// ---- UPDATE with expression ----
static void test_update_expression() {
    auto [tmp, table] = make_test_table();

    auto r = taql_execute("UPDATE t SET val = val * 2", table);
    check(r.affected_rows == 3, "update expr: 3 rows affected");

    auto cv0 = table.read_scalar_cell("val", 0);
    auto cv1 = table.read_scalar_cell("val", 1);
    auto cv2 = table.read_scalar_cell("val", 2);

    if (auto* d = std::get_if<double>(&cv0))
        check(std::abs(*d - 20.0) < 1e-10, "update expr: row 0 doubled");
    if (auto* d = std::get_if<double>(&cv1))
        check(std::abs(*d - 40.0) < 1e-10, "update expr: row 1 doubled");
    if (auto* d = std::get_if<double>(&cv2))
        check(std::abs(*d - 60.0) < 1e-10, "update expr: row 2 doubled");

    fs::remove_all(tmp);
}

// ---- INSERT then SELECT ----
static void test_insert_then_select() {
    auto [tmp, table] = make_test_table();

    (void)taql_execute("INSERT INTO t (id, val, name) VALUES (4, 40.0, 'dave')", table);
    auto r = taql_execute("SELECT val FROM t WHERE id == 4", table);
    check(r.rows.size() == 1, "insert+select: found 1 row");
    check(std::abs(as_dbl(r.values[0]) - 40.0) < 1e-10, "insert+select: val=40");

    fs::remove_all(tmp);
}

// ---- DELETE then COUNT ----
static void test_delete_then_count() {
    auto [tmp, table] = make_test_table();

    (void)taql_execute("DELETE FROM t WHERE id == 1", table);
    auto r = taql_execute("COUNT FROM t", table);
    check(as_i64(r.values[0]) == 2, "delete+count: 2 remaining");

    fs::remove_all(tmp);
}

int main() { // NOLINT(bugprone-exception-escape)
    test_insert_values();
    test_insert_multiple_rows();
    test_delete_where();
    test_delete_all();
    test_delete_limit();
    test_update();
    test_update_expression();
    test_insert_then_select();
    test_delete_then_count();

    std::cout << "taql_dml_test: " << check_count << " checks passed\n";
    return 0;
}
