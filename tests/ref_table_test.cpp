/// @file ref_table_test.cpp
/// @brief P12-W11 tests for RefTable (row-mapped view).

#include "casacore_mini/ref_table.hpp"
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
           ("ref_tbl_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) fs::remove_all(p);
}

static Table make_test_table(const fs::path& path) {
    std::vector<TableColumnSpec> cols = {
        {.name = "ID", .data_type = DataType::tp_int, .shape = {}},
        {.name = "VALUE", .data_type = DataType::tp_double, .shape = {}},
        {.name = "NAME", .data_type = DataType::tp_string, .shape = {}},
    };
    auto table = Table::create(path, cols, 5);
    for (int i = 0; i < 5; ++i) {
        auto row = static_cast<std::uint64_t>(i);
        table.write_scalar_cell("ID", row, CellValue(static_cast<std::int32_t>(i * 10)));
        table.write_scalar_cell("VALUE", row, CellValue(static_cast<double>(i) * 1.5));
        table.write_scalar_cell("NAME", row, CellValue(std::string("row_") + std::to_string(i)));
    }
    table.flush();
    return table;
}

// ============================================================================
// Test: Construct from row indices
// ============================================================================
static void test_row_indices_ctor() {
    auto path = make_temp_dir("idx");
    cleanup(path);
    auto table = make_test_table(path);

    std::vector<std::uint64_t> indices = {1, 3, 4};
    RefTable ref(table, indices);

    check(ref.nrow() == 3, "nrow is 3");
    check(ref.ncolumn() == 3, "ncolumn same as base");
    check(ref.columns().size() == 3, "columns size matches");

    // Row 0 in view → row 1 in base
    auto val = std::get<std::int32_t>(ref.read_scalar_cell("ID", 0));
    check(val == 10, "view row 0 → base row 1, ID=10");

    // Row 1 in view → row 3 in base
    val = std::get<std::int32_t>(ref.read_scalar_cell("ID", 1));
    check(val == 30, "view row 1 → base row 3, ID=30");

    // Row 2 in view → row 4 in base
    auto dval = std::get<double>(ref.read_scalar_cell("VALUE", 2));
    check(dval == 6.0, "view row 2 → base row 4, VALUE=6.0");

    check(ref.row_indices().size() == 3, "row_indices size");
    check(ref.base_row(0) == 1, "base_row(0) == 1");
    check(ref.base_row(2) == 4, "base_row(2) == 4");

    cleanup(path);
}

// ============================================================================
// Test: Construct from boolean mask
// ============================================================================
static void test_bool_mask_ctor() {
    auto path = make_temp_dir("mask");
    cleanup(path);
    auto table = make_test_table(path);

    std::vector<bool> mask = {true, false, true, false, true};
    RefTable ref(table, mask);

    check(ref.nrow() == 3, "mask selects 3 rows");
    check(ref.base_row(0) == 0, "first selected row is 0");
    check(ref.base_row(1) == 2, "second selected row is 2");
    check(ref.base_row(2) == 4, "third selected row is 4");

    auto name = std::get<std::string>(ref.read_scalar_cell("NAME", 1));
    check(name == "row_2", "view row 1 → base row 2, NAME=row_2");

    cleanup(path);
}

// ============================================================================
// Test: Empty view constructor
// ============================================================================
static void test_empty_ctor() {
    auto path = make_temp_dir("empty");
    cleanup(path);
    auto table = make_test_table(path);

    RefTable ref(table);

    check(ref.nrow() == 0, "empty view has 0 rows");
    check(ref.ncolumn() == 3, "columns still available");

    ref.add_row(2);
    ref.add_row(0);
    check(ref.nrow() == 2, "after add_row, nrow is 2");

    auto val = std::get<std::int32_t>(ref.read_scalar_cell("ID", 0));
    check(val == 20, "first added row (base 2) → ID=20");

    val = std::get<std::int32_t>(ref.read_scalar_cell("ID", 1));
    check(val == 0, "second added row (base 0) → ID=0");

    cleanup(path);
}

// ============================================================================
// Test: Out-of-range access
// ============================================================================
static void test_out_of_range() {
    auto path = make_temp_dir("oor");
    cleanup(path);
    auto table = make_test_table(path);

    RefTable ref(table, std::vector<std::uint64_t>{0, 2});

    bool threw = false;
    try {
        (void)ref.read_scalar_cell("ID", 5);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    check(threw, "out-of-range view row throws");

    cleanup(path);
}

// ============================================================================
// Test: base_table() accessor
// ============================================================================
static void test_base_table() {
    auto path = make_temp_dir("base");
    cleanup(path);
    auto table = make_test_table(path);

    RefTable ref(table, std::vector<std::uint64_t>{0});
    check(&ref.base_table() == &table, "base_table returns reference to original");

    const auto& cref = ref;
    check(&cref.base_table() == &table, "const base_table works");

    cleanup(path);
}

// ============================================================================
// Test: table_name and keywords delegation
// ============================================================================
static void test_delegation() {
    auto path = make_temp_dir("deleg");
    cleanup(path);
    auto table = make_test_table(path);

    RefTable ref(table, std::vector<std::uint64_t>{0, 1});
    check(ref.table_name() == table.table_name(), "table_name delegated");
    // keywords() should return same reference
    check(&ref.keywords() == &table.keywords(), "keywords delegated to base");

    cleanup(path);
}

int main() {
    test_row_indices_ctor();
    test_bool_mask_ctor();
    test_empty_ctor();
    test_out_of_range();
    test_base_table();
    test_delegation();

    std::cout << "ref_table_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
