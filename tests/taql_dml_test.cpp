// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

namespace {

fs::path temp_dir() {
    auto p = fs::temp_directory_path() / "casacore_mini_taql_dml_test";
    fs::create_directories(p);
    return p;
}

void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

/// Create a simple test table with 3 scalar columns and 5 rows.
Table make_test_table(const fs::path& dir) {
    cleanup(dir);
    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"NAME", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"VALUE", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 5);
    for (int i = 0; i < 5; ++i) {
        table.write_scalar_cell("ID", static_cast<std::uint64_t>(i),
                                CellValue(static_cast<std::int32_t>(i + 1)));
        table.write_scalar_cell("NAME", static_cast<std::uint64_t>(i),
                                CellValue(std::string("item_" + std::to_string(i + 1))));
        table.write_scalar_cell("VALUE", static_cast<std::uint64_t>(i),
                                CellValue(static_cast<double>(i + 1) * 10.0));
    }
    table.flush();
    return table;
}

// --- INSERT tests ---

void test_insert_values() {
    auto dir = temp_dir() / "insert_values";
    auto table = make_test_table(dir);
    assert(table.nrow() == 5);

    auto result = taql_execute("INSERT INTO t [ID, NAME, VALUE] VALUES (6, 'item_6', 60.0)", table);
    assert(result.affected_rows == 1);
    assert(table.nrow() == 6);

    // Verify the inserted row.
    auto id = std::get<std::int32_t>(table.read_scalar_cell("ID", 5));
    (void)id;
    assert(id == 6);
    auto name = std::get<std::string>(table.read_scalar_cell("NAME", 5));
    (void)name;
    assert(name == "item_6");
    auto val = std::get<double>(table.read_scalar_cell("VALUE", 5));
    (void)val;
    assert(val == 60.0);

    cleanup(dir);
}

void test_insert_multiple_values() {
    auto dir = temp_dir() / "insert_multi";
    auto table = make_test_table(dir);

    auto result = taql_execute(
        "INSERT INTO t [ID, NAME, VALUE] VALUES (6, 'a', 60.0), (7, 'b', 70.0)", table);
    assert(result.affected_rows == 2);
    assert(table.nrow() == 7);

    auto id5 = std::get<std::int32_t>(table.read_scalar_cell("ID", 5));
    (void)id5;
    assert(id5 == 6);
    auto id6 = std::get<std::int32_t>(table.read_scalar_cell("ID", 6));
    (void)id6;
    assert(id6 == 7);

    cleanup(dir);
}

void test_insert_set() {
    auto dir = temp_dir() / "insert_set";
    auto table = make_test_table(dir);

    auto result = taql_execute("INSERT INTO t SET ID=10, NAME='new', VALUE=100.0", table);
    assert(result.affected_rows == 1);
    assert(table.nrow() == 6);

    auto id = std::get<std::int32_t>(table.read_scalar_cell("ID", 5));
    (void)id;
    assert(id == 10);

    cleanup(dir);
}

void test_insert_column_count_mismatch() {
    auto dir = temp_dir() / "insert_mismatch";
    auto table = make_test_table(dir);

    bool threw = false;
    try {
        (void)taql_execute("INSERT INTO t [ID, NAME] VALUES (1, 'x', 99.0)", table);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    (void)threw;
    assert(threw);

    cleanup(dir);
}

void test_insert_readonly_throws() {
    auto dir = temp_dir() / "insert_ro";
    auto table = make_test_table(dir);
    table.flush();

    auto ro = Table::open(dir);
    bool threw = false;
    try {
        (void)taql_execute("INSERT INTO t [ID, NAME, VALUE] VALUES (1, 'x', 1.0)", ro);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    (void)threw;
    assert(threw);

    cleanup(dir);
}

// --- DELETE tests ---

void test_delete_where() {
    auto dir = temp_dir() / "delete_where";
    auto table = make_test_table(dir);
    assert(table.nrow() == 5);

    // Delete rows where ID > 3 (rows 3, 4 = IDs 4, 5).
    auto result = taql_execute("DELETE FROM t WHERE ID > 3", table);
    assert(result.affected_rows == 2);
    assert(table.nrow() == 3);

    // Verify remaining rows.
    auto id0 = std::get<std::int32_t>(table.read_scalar_cell("ID", 0));
    (void)id0;
    assert(id0 == 1);
    auto id2 = std::get<std::int32_t>(table.read_scalar_cell("ID", 2));
    (void)id2;
    assert(id2 == 3);

    cleanup(dir);
}

void test_delete_all() {
    auto dir = temp_dir() / "delete_all";
    auto table = make_test_table(dir);

    // DELETE without WHERE removes all rows.
    auto result = taql_execute("DELETE FROM t", table);
    assert(result.affected_rows == 5);
    assert(table.nrow() == 0);

    cleanup(dir);
}

void test_delete_with_limit() {
    auto dir = temp_dir() / "delete_limit";
    auto table = make_test_table(dir);

    // Delete at most 2 rows where ID > 1.
    auto result = taql_execute("DELETE FROM t WHERE ID > 1 LIMIT 2", table);
    assert(result.affected_rows == 2);
    assert(table.nrow() == 3);

    cleanup(dir);
}

void test_delete_no_match() {
    auto dir = temp_dir() / "delete_nomatch";
    auto table = make_test_table(dir);

    auto result = taql_execute("DELETE FROM t WHERE ID > 100", table);
    assert(result.affected_rows == 0);
    assert(table.nrow() == 5);

    cleanup(dir);
}

void test_delete_readonly_throws() {
    auto dir = temp_dir() / "delete_ro";
    auto table = make_test_table(dir);
    table.flush();

    auto ro = Table::open(dir);
    bool threw = false;
    try {
        (void)taql_execute("DELETE FROM t WHERE ID = 1", ro);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    (void)threw;
    assert(threw);

    cleanup(dir);
}

// --- Combined INSERT + DELETE test ---

void test_insert_then_delete() {
    auto dir = temp_dir() / "insert_delete";
    auto table = make_test_table(dir);
    assert(table.nrow() == 5);

    // Insert 2 rows.
    (void)taql_execute(
        "INSERT INTO t [ID, NAME, VALUE] VALUES (10, 'ten', 100.0), (20, 'twenty', 200.0)", table);
    assert(table.nrow() == 7);

    // Delete the ones we just inserted.
    auto result = taql_execute("DELETE FROM t WHERE ID >= 10", table);
    assert(result.affected_rows == 2);
    assert(table.nrow() == 5);

    // Original rows still intact.
    for (int i = 0; i < 5; ++i) {
        auto id =
            std::get<std::int32_t>(table.read_scalar_cell("ID", static_cast<std::uint64_t>(i)));
        (void)id;
        assert(id == i + 1);
    }

    cleanup(dir);
}

} // namespace

int main() {
    test_insert_values();
    test_insert_multiple_values();
    test_insert_set();
    test_insert_column_count_mismatch();
    test_insert_readonly_throws();
    test_delete_where();
    test_delete_all();
    test_delete_with_limit();
    test_delete_no_match();
    test_delete_readonly_throws();
    test_insert_then_delete();
    return 0;
}
