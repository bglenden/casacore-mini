#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

namespace {

fs::path temp_dir() {
    auto p = fs::temp_directory_path() / "casacore_mini_taql_join_test";
    fs::create_directories(p);
    return p;
}

void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

/// Create the "departments" table: DEPT_ID, DEPT_NAME
Table make_dept_table(const fs::path& dir) {
    cleanup(dir);
    std::vector<TableColumnSpec> cols = {
        {"DEPT_ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"DEPT_NAME", DataType::tp_string, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(dir, cols, 3);
    table.write_scalar_cell("DEPT_ID", 0, CellValue(std::int32_t(1)));
    table.write_scalar_cell("DEPT_NAME", 0, CellValue(std::string("Engineering")));
    table.write_scalar_cell("DEPT_ID", 1, CellValue(std::int32_t(2)));
    table.write_scalar_cell("DEPT_NAME", 1, CellValue(std::string("Marketing")));
    table.write_scalar_cell("DEPT_ID", 2, CellValue(std::int32_t(3)));
    table.write_scalar_cell("DEPT_NAME", 2, CellValue(std::string("Sales")));
    table.flush();
    return table;
}

/// Create the "employees" table: EMP_ID, EMP_NAME, DEPT_ID
Table make_emp_table(const fs::path& dir) {
    cleanup(dir);
    std::vector<TableColumnSpec> cols = {
        {"EMP_ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"EMP_NAME", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"DEPT_ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(dir, cols, 4);
    table.write_scalar_cell("EMP_ID", 0, CellValue(std::int32_t(101)));
    table.write_scalar_cell("EMP_NAME", 0, CellValue(std::string("Alice")));
    table.write_scalar_cell("DEPT_ID", 0, CellValue(std::int32_t(1)));

    table.write_scalar_cell("EMP_ID", 1, CellValue(std::int32_t(102)));
    table.write_scalar_cell("EMP_NAME", 1, CellValue(std::string("Bob")));
    table.write_scalar_cell("DEPT_ID", 1, CellValue(std::int32_t(2)));

    table.write_scalar_cell("EMP_ID", 2, CellValue(std::int32_t(103)));
    table.write_scalar_cell("EMP_NAME", 2, CellValue(std::string("Carol")));
    table.write_scalar_cell("DEPT_ID", 2, CellValue(std::int32_t(1)));

    table.write_scalar_cell("EMP_ID", 3, CellValue(std::int32_t(104)));
    table.write_scalar_cell("EMP_NAME", 3, CellValue(std::string("Dave")));
    table.write_scalar_cell("DEPT_ID", 3, CellValue(std::int32_t(3)));

    table.flush();
    return table;
}

// --- Multi-FROM cross-product ---

void test_multi_from_cross_product() {
    auto dept_dir = temp_dir() / "cross_dept";
    auto emp_dir = temp_dir() / "cross_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    // Cross product: 4 employees * 3 departments = 12 rows without WHERE.
    auto result = taql_execute(
        "SELECT e.EMP_NAME, d.DEPT_NAME FROM e, d", tables);

    // 12 combos * 2 values each = 24 values.
    assert(result.values.size() == 24);

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- JOIN ... ON ---

void test_join_on() {
    auto dept_dir = temp_dir() / "join_dept";
    auto emp_dir = temp_dir() / "join_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    auto result = taql_execute(
        "SELECT e.EMP_NAME, d.DEPT_NAME FROM e JOIN d ON e.DEPT_ID = d.DEPT_ID",
        tables);

    // 4 employees each matched to their department = 4 result rows.
    assert(result.column_names.size() == 2);
    assert(result.values.size() == 8);  // 4 rows * 2 cols

    // Verify the join correctness:
    // Alice -> Engineering, Bob -> Marketing, Carol -> Engineering, Dave -> Sales
    auto name0 = std::get<std::string>(result.values[0]);
    auto dept0 = std::get<std::string>(result.values[1]);
    (void)name0; (void)dept0;
    assert(name0 == "Alice");
    assert(dept0 == "Engineering");

    auto name1 = std::get<std::string>(result.values[2]);
    auto dept1 = std::get<std::string>(result.values[3]);
    (void)name1; (void)dept1;
    assert(name1 == "Bob");
    assert(dept1 == "Marketing");

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- JOIN with WHERE ---

void test_join_with_where() {
    auto dept_dir = temp_dir() / "joinw_dept";
    auto emp_dir = temp_dir() / "joinw_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    // Join and filter: only Engineering employees.
    auto result = taql_execute(
        "SELECT e.EMP_NAME FROM e JOIN d ON e.DEPT_ID = d.DEPT_ID "
        "WHERE d.DEPT_NAME = 'Engineering'",
        tables);

    assert(result.values.size() == 2);  // Alice and Carol
    auto n0 = std::get<std::string>(result.values[0]);
    auto n1 = std::get<std::string>(result.values[1]);
    (void)n0; (void)n1;
    assert(n0 == "Alice");
    assert(n1 == "Carol");

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- Multi-FROM with WHERE (implicit join) ---

void test_multi_from_with_where() {
    auto dept_dir = temp_dir() / "mf_dept";
    auto emp_dir = temp_dir() / "mf_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    // Implicit join via WHERE clause.
    auto result = taql_execute(
        "SELECT e.EMP_NAME, d.DEPT_NAME FROM e, d WHERE e.DEPT_ID = d.DEPT_ID",
        tables);

    // Same result as explicit JOIN: 4 matching combos.
    assert(result.values.size() == 8);  // 4 rows * 2 cols

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- JOIN with LIMIT ---

void test_join_with_limit() {
    auto dept_dir = temp_dir() / "joinl_dept";
    auto emp_dir = temp_dir() / "joinl_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    auto result = taql_execute(
        "SELECT e.EMP_NAME FROM e JOIN d ON e.DEPT_ID = d.DEPT_ID LIMIT 2",
        tables);

    assert(result.values.size() == 2);

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- Empty join result ---

void test_join_no_match() {
    auto dept_dir = temp_dir() / "joine_dept";
    auto emp_dir = temp_dir() / "joine_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    // No employee has DEPT_ID=99.
    auto result = taql_execute(
        "SELECT e.EMP_NAME FROM e JOIN d ON e.DEPT_ID = d.DEPT_ID "
        "WHERE d.DEPT_ID = 99",
        tables);

    assert(result.values.empty());

    cleanup(dept_dir);
    cleanup(emp_dir);
}

// --- Wildcard with multi-table ---

void test_multi_table_wildcard() {
    auto dept_dir = temp_dir() / "wild_dept";
    auto emp_dir = temp_dir() / "wild_emp";
    auto dept = make_dept_table(dept_dir);
    auto emp = make_emp_table(emp_dir);

    std::unordered_map<std::string, Table*> tables = {
        {"e", &emp},
        {"d", &dept},
    };

    auto result = taql_execute(
        "SELECT * FROM e JOIN d ON e.DEPT_ID = d.DEPT_ID LIMIT 1",
        tables);

    // emp has 3 cols, dept has 2 cols = 5 values per row, 1 row.
    assert(result.values.size() == 5);
    // Column names should be qualified: e.EMP_ID, e.EMP_NAME, e.DEPT_ID, d.DEPT_ID, d.DEPT_NAME
    assert(result.column_names.size() == 5);

    cleanup(dept_dir);
    cleanup(emp_dir);
}

} // namespace

int main() {
    test_multi_from_cross_product();
    test_join_on();
    test_join_with_where();
    test_multi_from_with_where();
    test_join_with_limit();
    test_join_no_match();
    test_multi_table_wildcard();
    return 0;
}
