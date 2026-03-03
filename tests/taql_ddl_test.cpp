// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

namespace {

fs::path temp_dir() {
    auto p = fs::temp_directory_path() / "casacore_mini_taql_ddl_test";
    fs::create_directories(p);
    return p;
}

void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

/// Create a simple writable test table for ALTER/DROP tests.
Table make_test_table(const fs::path& dir) {
    cleanup(dir);
    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"NAME", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"VALUE", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 3);
    for (int i = 0; i < 3; ++i) {
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

// --- CREATE TABLE tests ---

void test_create_table_basic() {
    auto dir = temp_dir() / "create_basic";
    cleanup(dir);

    auto result = taql_calc("CREATE TABLE '" + dir.string() + "' (X INT, Y DOUBLE, Z STRING)");
    assert(result.output_table == dir.string());

    // Verify the table was created on disk.
    assert(fs::exists(dir));

    auto table = Table::open(dir);
    assert(table.nrow() == 0);
    assert(table.ncolumn() == 3);

    const auto& cols = table.columns();
    assert(cols.size() == 3);
    assert(cols[0].name == "X");
    assert(cols[1].name == "Y");
    assert(cols[2].name == "Z");

    cleanup(dir);
}

void test_create_table_type_aliases() {
    auto dir = temp_dir() / "create_types";
    cleanup(dir);

    // Test various type aliases.
    (void)taql_calc("CREATE TABLE '" + dir.string() +
                    "' (a BOOL, b SHORT, c INT, d INT64, e FLOAT, f DOUBLE, g STRING)");
    assert(fs::exists(dir));

    auto table = Table::open(dir);
    assert(table.ncolumn() == 7);

    cleanup(dir);
}

void test_create_table_unknown_type_throws() {
    auto dir = temp_dir() / "create_badtype";
    cleanup(dir);

    bool threw = false;
    try {
        (void)taql_calc("CREATE TABLE '" + dir.string() + "' (X FOOBAR)");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    (void)threw;
    assert(threw);

    cleanup(dir);
}

// --- ALTER TABLE tests ---

void test_alter_add_row() {
    auto dir = temp_dir() / "alter_addrow";
    auto table = make_test_table(dir);
    assert(table.nrow() == 3);

    auto result = taql_execute("ALTER TABLE t ADDROW 2", table);
    assert(result.affected_rows == 2);
    assert(table.nrow() == 5);

    cleanup(dir);
}

void test_alter_set_keyword() {
    auto dir = temp_dir() / "alter_setkw";
    auto table = make_test_table(dir);

    (void)taql_execute("ALTER TABLE t SET KEYWORD version=42, author='test'", table);

    const auto& kw = table.keywords();
    auto* ver = kw.find("version");
    assert(ver != nullptr);
    auto* vi = std::get_if<std::int32_t>(&ver->storage());
    assert(vi != nullptr);
    (void)vi;
    assert(*vi == 42);

    auto* auth = kw.find("author");
    assert(auth != nullptr);
    auto* as = std::get_if<std::string>(&auth->storage());
    assert(as != nullptr);
    (void)as;
    assert(*as == "test");

    cleanup(dir);
}

void test_alter_drop_keyword() {
    auto dir = temp_dir() / "alter_dropkw";
    auto table = make_test_table(dir);

    // Set a keyword, then drop it.
    (void)taql_execute("ALTER TABLE t SET KEYWORD mykey='hello'", table);
    assert(table.keywords().find("mykey") != nullptr);

    (void)taql_execute("ALTER TABLE t DROPKEY mykey", table);
    assert(table.keywords().find("mykey") == nullptr);

    cleanup(dir);
}

void test_alter_readonly_throws() {
    auto dir = temp_dir() / "alter_ro";
    auto table = make_test_table(dir);
    table.flush();

    auto ro = Table::open(dir);
    bool threw = false;
    try {
        (void)taql_execute("ALTER TABLE t ADDROW 1", ro);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    (void)threw;
    assert(threw);

    cleanup(dir);
}

// --- DROP TABLE tests ---

void test_drop_table() {
    auto dir = temp_dir() / "drop_test";
    auto table = make_test_table(dir);
    table.flush();

    assert(fs::exists(dir));

    // Use taql_calc for DROP since it doesn't need a table reference.
    (void)taql_calc("DROP TABLE '" + dir.string() + "'");
    assert(!fs::exists(dir));
}

void test_drop_nonexistent_is_silent() {
    auto dir = temp_dir() / "drop_noexist";
    cleanup(dir);

    // Should not throw for nonexistent paths.
    (void)taql_calc("DROP TABLE '" + dir.string() + "'");
}

// --- Record::remove test ---

void test_record_remove() {
    Record rec;
    rec.set("a", RecordValue(std::int32_t(1)));
    rec.set("b", RecordValue(std::int32_t(2)));
    rec.set("c", RecordValue(std::int32_t(3)));
    assert(rec.size() == 3);

    bool removed = rec.remove("b");
    assert(removed);
    assert(rec.size() == 2);
    assert(rec.find("b") == nullptr);
    assert(rec.find("a") != nullptr);
    assert(rec.find("c") != nullptr);

    // Removing nonexistent key returns false.
    bool removed2 = rec.remove("nonexistent");
    assert(!removed2);
    assert(rec.size() == 2);
}

} // namespace

int main() {
    test_create_table_basic();
    test_create_table_type_aliases();
    test_create_table_unknown_type_throws();
    test_alter_add_row();
    test_alter_set_keyword();
    test_alter_drop_keyword();
    test_alter_readonly_throws();
    test_drop_table();
    test_drop_nonexistent_is_silent();
    test_record_remove();
    return 0;
}
