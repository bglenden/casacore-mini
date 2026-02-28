#include "casacore_mini/taql.hpp"
#include "casacore_mini/table.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

// ---- CREATE TABLE ----
static void test_create_table() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_create_test";
    fs::remove_all(tmp);

    // CREATE TABLE with columns and LIMIT (initial rows)
    auto r = taql_calc("CREATE TABLE '" + tmp.string() +
                        "' (col_a DOUBLE, col_b INT, col_c STRING) LIMIT 5");

    check(fs::exists(tmp), "create: table directory exists");

    // Open and verify
    auto table = Table::open(tmp);
    check(table.nrow() == 5, "create: 5 initial rows");
    auto cols = table.columns();
    check(cols.size() == 3, "create: 3 columns");

    // Verify column types
    bool found_a = false;
    bool found_b = false;
    bool found_c = false;
    for (auto& c : cols) {
        if (c.name == "col_a") { found_a = true; check(c.data_type == DataType::tp_double, "create: col_a is double"); }
        if (c.name == "col_b") { found_b = true; check(c.data_type == DataType::tp_int, "create: col_b is int"); }
        if (c.name == "col_c") { found_c = true; check(c.data_type == DataType::tp_string, "create: col_c is string"); }
    }
    check(found_a && found_b && found_c, "create: all columns found");

    fs::remove_all(tmp);
}

// ---- CREATE TABLE with 0 rows ----
static void test_create_table_empty() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_create_empty_test";
    fs::remove_all(tmp);

    auto r = taql_calc("CREATE TABLE '" + tmp.string() + "' (x DOUBLE)");

    auto table = Table::open(tmp);
    check(table.nrow() == 0, "create empty: 0 rows");
    check(table.columns().size() == 1, "create empty: 1 column");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: ADD COLUMN ----
static void test_alter_add_column() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_add_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 3);
    table.flush();

    auto r = taql_execute("ALTER TABLE t ADD COLUMN (new_col DOUBLE)", table);

    auto updated_cols = table.columns();
    check(updated_cols.size() == 2, "alter add: 2 columns");
    bool found = false;
    for (auto& c : updated_cols)
        if (c.name == "new_col") found = true;
    check(found, "alter add: new_col found");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: RENAME COLUMN ----
static void test_alter_rename_column() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_rename_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"old_name", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 2);
    table.write_scalar_cell("old_name", 0, 42.0);
    table.flush();

    auto r = taql_execute("ALTER TABLE t RENAME COLUMN old_name TO new_name", table);

    auto updated_cols = table.columns();
    bool found_new = false;
    bool found_old = false;
    for (auto& c : updated_cols) {
        if (c.name == "new_name") found_new = true;
        if (c.name == "old_name") found_old = true;
    }
    check(found_new, "alter rename: new_name found");
    check(!found_old, "alter rename: old_name gone");

    // Data preserved
    auto cv = table.read_scalar_cell("new_name", 0);
    if (auto* d = std::get_if<double>(&cv))
        check(std::abs(*d - 42.0) < 1e-10, "alter rename: data preserved");
    else
        check(false, "alter rename: data type preserved");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: DROP COLUMN ----
static void test_alter_drop_column() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_drop_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"keep", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"remove_me", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 1);
    table.flush();

    auto r = taql_execute("ALTER TABLE t DROP COLUMN remove_me", table);

    auto updated_cols = table.columns();
    check(updated_cols.size() == 1, "alter drop: 1 column left");
    check(updated_cols[0].name == "keep", "alter drop: correct column remains");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: SET KEYWORD ----
static void test_alter_set_keyword() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_kw_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 0);
    table.flush();

    auto r = taql_execute("ALTER TABLE t SET KEYWORD my_key = 42", table);

    auto* kw = table.keywords().find("my_key");
    check(kw != nullptr, "alter set kw: keyword exists");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: RENAME KEYWORD ----
static void test_alter_rename_keyword() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_rkw_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 0);
    table.set_keyword("old_kw", RecordValue(std::string("hello")));
    table.flush();

    auto r = taql_execute("ALTER TABLE t RENAMEKEY old_kw TO new_kw", table);

    check(table.keywords().find("old_kw") == nullptr, "alter rename kw: old gone");
    check(table.keywords().find("new_kw") != nullptr, "alter rename kw: new exists");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: DROP KEYWORD ----
static void test_alter_drop_keyword() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_dkw_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 0);
    table.set_keyword("to_remove", RecordValue(1.0));
    table.flush();

    auto r = taql_execute("ALTER TABLE t DROPKEY to_remove", table);

    check(table.keywords().find("to_remove") == nullptr, "alter drop kw: gone");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: ADD ROW ----
static void test_alter_add_row() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_addrow_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 2);
    table.flush();

    check(table.nrow() == 2, "alter addrow: initial 2 rows");

    auto r = taql_execute("ALTER TABLE t ADDROW 3", table);

    check(table.nrow() == 5, "alter addrow: now 5 rows");

    fs::remove_all(tmp);
}

// ---- ALTER TABLE: COPY COLUMN ----
static void test_alter_copy_column() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_alter_copy_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"src", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 2);
    table.write_scalar_cell("src", 0, 10.0);
    table.write_scalar_cell("src", 1, 20.0);
    table.flush();

    auto r = taql_execute("ALTER TABLE t COPY COLUMN src TO dst_copy", table);

    // dst_copy should now exist as a new column with copied data
    auto updated_cols = table.columns();
    bool found_dst = false;
    for (auto& c : updated_cols)
        if (c.name == "dst_copy") found_dst = true;
    check(found_dst, "alter copy: dst_copy column created");

    auto v0 = table.read_scalar_cell("dst_copy", 0);
    auto v1 = table.read_scalar_cell("dst_copy", 1);
    if (auto* d0 = std::get_if<double>(&v0))
        check(std::abs(*d0 - 10.0) < 1e-10, "alter copy: row 0");
    if (auto* d1 = std::get_if<double>(&v1))
        check(std::abs(*d1 - 20.0) < 1e-10, "alter copy: row 1");

    fs::remove_all(tmp);
}

// ---- DROP TABLE ----
static void test_drop_table() {
    auto tmp = fs::temp_directory_path() / "taql_ddl_drop_test";
    fs::remove_all(tmp);

    // Create first
    std::vector<TableColumnSpec> cols = {
        {"x", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 0);
    table.flush();
    check(fs::exists(tmp), "drop: table exists before drop");

    // Drop via taql_calc
    auto r = taql_calc("DROP TABLE '" + tmp.string() + "'");

    check(!fs::exists(tmp), "drop: table removed after drop");
}

int main() { // NOLINT(bugprone-exception-escape)
    test_create_table();
    test_create_table_empty();
    test_alter_add_column();
    test_alter_rename_column();
    test_alter_drop_column();
    test_alter_set_keyword();
    test_alter_rename_keyword();
    test_alter_drop_keyword();
    test_alter_add_row();
    test_alter_copy_column();
    test_drop_table();

    std::cout << "taql_ddl_test: " << check_count << " checks passed\n";
    return 0;
}
