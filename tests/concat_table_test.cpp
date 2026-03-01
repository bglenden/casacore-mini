/// @file concat_table_test.cpp
/// @brief P12-W11 tests for ConcatTable (virtual concatenation).

#include "casacore_mini/concat_table.hpp"
#include "casacore_mini/table.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace casacore_mini;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "FAIL: " << label << "\n";
    }
}

static fs::path make_temp_dir(const std::string& suffix) {
    return fs::temp_directory_path() /
           ("concat_tbl_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p))
        fs::remove_all(p);
}

static Table make_table(const fs::path& path, int start, int count) {
    std::vector<TableColumnSpec> cols = {
        {.name = "ID", .data_type = DataType::tp_int, .shape = {}},
        {.name = "VAL", .data_type = DataType::tp_double, .shape = {}},
    };
    auto table = Table::create(path, cols, static_cast<std::uint64_t>(count));
    for (int i = 0; i < count; ++i) {
        auto row = static_cast<std::uint64_t>(i);
        table.write_scalar_cell("ID", row, CellValue(static_cast<std::int32_t>(start + i)));
        table.write_scalar_cell("VAL", row, CellValue(static_cast<double>(start + i) * 0.5));
    }
    table.flush();
    return table;
}

// ============================================================================
// Test: Basic concatenation of two tables
// ============================================================================
static void test_basic_concat() {
    auto p1 = make_temp_dir("a");
    auto p2 = make_temp_dir("b");
    cleanup(p1);
    cleanup(p2);

    auto t1 = make_table(p1, 0, 3);  // rows 0,1,2 → IDs 0,1,2
    auto t2 = make_table(p2, 10, 2); // rows 0,1   → IDs 10,11

    ConcatTable ct({&t1, &t2});

    check(ct.nrow() == 5, "total rows = 3 + 2");
    check(ct.ncolumn() == 2, "ncolumn from first table");
    check(ct.table_count() == 2, "2 component tables");

    // Read through concatenated view
    auto id0 = std::get<std::int32_t>(ct.read_scalar_cell("ID", 0));
    check(id0 == 0, "row 0 → table1 row 0, ID=0");

    auto id2 = std::get<std::int32_t>(ct.read_scalar_cell("ID", 2));
    check(id2 == 2, "row 2 → table1 row 2, ID=2");

    auto id3 = std::get<std::int32_t>(ct.read_scalar_cell("ID", 3));
    check(id3 == 10, "row 3 → table2 row 0, ID=10");

    auto id4 = std::get<std::int32_t>(ct.read_scalar_cell("ID", 4));
    check(id4 == 11, "row 4 → table2 row 1, ID=11");

    auto val3 = std::get<double>(ct.read_scalar_cell("VAL", 3));
    check(val3 == 5.0, "row 3 → table2 row 0, VAL=5.0");

    cleanup(p1);
    cleanup(p2);
}

// ============================================================================
// Test: decompose_row mapping
// ============================================================================
static void test_decompose_row() {
    auto p1 = make_temp_dir("d1");
    auto p2 = make_temp_dir("d2");
    auto p3 = make_temp_dir("d3");
    cleanup(p1);
    cleanup(p2);
    cleanup(p3);

    auto t1 = make_table(p1, 0, 2);
    auto t2 = make_table(p2, 0, 3);
    auto t3 = make_table(p3, 0, 1);

    ConcatTable ct({&t1, &t2, &t3});
    check(ct.nrow() == 6, "total 6 rows");

    auto [ti0, lr0] = ct.decompose_row(0);
    check(ti0 == 0 && lr0 == 0, "row 0 → table 0, local 0");

    auto [ti1, lr1] = ct.decompose_row(1);
    check(ti1 == 0 && lr1 == 1, "row 1 → table 0, local 1");

    auto [ti2, lr2] = ct.decompose_row(2);
    check(ti2 == 1 && lr2 == 0, "row 2 → table 1, local 0");

    auto [ti4, lr4] = ct.decompose_row(4);
    check(ti4 == 1 && lr4 == 2, "row 4 → table 1, local 2");

    auto [ti5, lr5] = ct.decompose_row(5);
    check(ti5 == 2 && lr5 == 0, "row 5 → table 2, local 0");

    cleanup(p1);
    cleanup(p2);
    cleanup(p3);
}

// ============================================================================
// Test: Out-of-range row
// ============================================================================
static void test_out_of_range() {
    auto p1 = make_temp_dir("oor");
    cleanup(p1);
    auto t1 = make_table(p1, 0, 2);

    ConcatTable ct({&t1});

    bool threw = false;
    try {
        (void)ct.decompose_row(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "out-of-range row throws");

    cleanup(p1);
}

// ============================================================================
// Test: Empty tables constructor throws
// ============================================================================
static void test_empty_throws() {
    bool threw = false;
    try {
        ConcatTable ct(std::vector<Table*>{});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "empty table list throws");
}

// ============================================================================
// Test: table_at accessor
// ============================================================================
static void test_table_at() {
    auto p1 = make_temp_dir("at1");
    auto p2 = make_temp_dir("at2");
    cleanup(p1);
    cleanup(p2);

    auto t1 = make_table(p1, 0, 2);
    auto t2 = make_table(p2, 0, 3);

    ConcatTable ct({&t1, &t2});
    check(&ct.table_at(0) == &t1, "table_at(0) returns first table");
    check(&ct.table_at(1) == &t2, "table_at(1) returns second table");

    bool threw = false;
    try {
        (void)ct.table_at(5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "table_at out-of-range throws");

    cleanup(p1);
    cleanup(p2);
}

// ============================================================================
// Test: columns and keywords from first table
// ============================================================================
static void test_columns_keywords() {
    auto p1 = make_temp_dir("ck1");
    auto p2 = make_temp_dir("ck2");
    cleanup(p1);
    cleanup(p2);

    auto t1 = make_table(p1, 0, 1);
    auto t2 = make_table(p2, 0, 1);

    ConcatTable ct({&t1, &t2});
    check(&ct.columns() == &t1.columns(), "columns from first table");
    check(&ct.keywords() == &t1.keywords(), "keywords from first table");

    cleanup(p1);
    cleanup(p2);
}

int main() {
    test_basic_concat();
    test_decompose_row();
    test_out_of_range();
    test_empty_throws();
    test_table_at();
    test_columns_keywords();

    std::cout << "concat_table_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
