// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file columns_index_test.cpp
/// @brief P12-W11 tests for ColumnsIndex (fast key lookup).

#include "casacore_mini/columns_index.hpp"
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
           ("col_idx_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p))
        fs::remove_all(p);
}

/// Create a table with SCAN and FIELD_ID columns, 6 rows:
///  row 0: SCAN=1, FIELD=0
///  row 1: SCAN=2, FIELD=0
///  row 2: SCAN=1, FIELD=1
///  row 3: SCAN=3, FIELD=0
///  row 4: SCAN=2, FIELD=1
///  row 5: SCAN=1, FIELD=0
static Table make_test_table(const fs::path& path) {
    std::vector<TableColumnSpec> cols = {
        {.name = "SCAN", .data_type = DataType::tp_int},
        {.name = "FIELD_ID", .data_type = DataType::tp_int},
    };
    auto table = Table::create(path, cols, 6);
    std::int32_t scans[] = {1, 2, 1, 3, 2, 1};
    std::int32_t fields[] = {0, 0, 1, 0, 1, 0};
    for (int i = 0; i < 6; ++i) {
        auto r = static_cast<std::uint64_t>(i);
        table.write_scalar_cell("SCAN", r, CellValue(scans[i]));
        table.write_scalar_cell("FIELD_ID", r, CellValue(fields[i]));
    }
    table.flush();
    return table;
}

// ============================================================================
// Test: Single-column index lookup
// ============================================================================
static void test_single_column_lookup() {
    auto path = make_temp_dir("single");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx(table, {"SCAN"});

    // SCAN=1 appears at rows 0,2,5
    Record key;
    key.set("SCAN", RecordValue(static_cast<std::int32_t>(1)));
    auto rows = idx.get_row_numbers(key);
    check(rows.size() == 3, "SCAN=1 has 3 rows");

    // SCAN=2 appears at rows 1,4
    Record key2;
    key2.set("SCAN", RecordValue(static_cast<std::int32_t>(2)));
    rows = idx.get_row_numbers(key2);
    check(rows.size() == 2, "SCAN=2 has 2 rows");

    // SCAN=3 appears at row 3
    Record key3;
    key3.set("SCAN", RecordValue(static_cast<std::int32_t>(3)));
    rows = idx.get_row_numbers(key3);
    check(rows.size() == 1, "SCAN=3 has 1 row");
    check(rows[0] == 3, "SCAN=3 is at row 3");

    cleanup(path);
}

// ============================================================================
// Test: Two-column index lookup
// ============================================================================
static void test_two_column_lookup() {
    auto path = make_temp_dir("two");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx(table, {"SCAN", "FIELD_ID"});

    // (SCAN=1, FIELD=0) → rows 0, 5
    Record key;
    key.set("SCAN", RecordValue(static_cast<std::int32_t>(1)));
    key.set("FIELD_ID", RecordValue(static_cast<std::int32_t>(0)));
    auto rows = idx.get_row_numbers(key);
    check(rows.size() == 2, "(SCAN=1,FIELD=0) has 2 rows");

    // (SCAN=1, FIELD=1) → row 2
    Record key2;
    key2.set("SCAN", RecordValue(static_cast<std::int32_t>(1)));
    key2.set("FIELD_ID", RecordValue(static_cast<std::int32_t>(1)));
    rows = idx.get_row_numbers(key2);
    check(rows.size() == 1, "(SCAN=1,FIELD=1) has 1 row");
    check(rows[0] == 2, "(SCAN=1,FIELD=1) is at row 2");

    cleanup(path);
}

// ============================================================================
// Test: unique_key_count
// ============================================================================
static void test_unique_key_count() {
    auto path = make_temp_dir("count");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx_scan(table, {"SCAN"});
    check(idx_scan.unique_key_count() == 3, "3 unique SCAN values");

    ColumnsIndex idx_both(table, {"SCAN", "FIELD_ID"});
    // Unique pairs: (1,0), (2,0), (1,1), (3,0), (2,1) = 5
    check(idx_both.unique_key_count() == 5, "5 unique (SCAN,FIELD) pairs");

    cleanup(path);
}

// ============================================================================
// Test: contains
// ============================================================================
static void test_contains() {
    auto path = make_temp_dir("contains");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx(table, {"SCAN"});

    Record k1;
    k1.set("SCAN", RecordValue(static_cast<std::int32_t>(1)));
    check(idx.contains(k1), "contains SCAN=1");

    Record k_missing;
    k_missing.set("SCAN", RecordValue(static_cast<std::int32_t>(99)));
    check(!idx.contains(k_missing), "does not contain SCAN=99");

    cleanup(path);
}

// ============================================================================
// Test: get_row_number (unique key)
// ============================================================================
static void test_get_row_number_unique() {
    auto path = make_temp_dir("unique");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx(table, {"SCAN"});

    // SCAN=3 is unique → row 3
    Record key;
    key.set("SCAN", RecordValue(static_cast<std::int32_t>(3)));
    auto row = idx.get_row_number(key);
    check(row == 3, "get_row_number SCAN=3 → row 3");

    // SCAN=1 is not unique → should throw
    Record key_dup;
    key_dup.set("SCAN", RecordValue(static_cast<std::int32_t>(1)));
    bool threw = false;
    try {
        (void)idx.get_row_number(key_dup);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "get_row_number on non-unique key throws");

    // Missing key → should throw
    Record key_miss;
    key_miss.set("SCAN", RecordValue(static_cast<std::int32_t>(999)));
    threw = false;
    try {
        (void)idx.get_row_number(key_miss);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "get_row_number on missing key throws");

    cleanup(path);
}

// ============================================================================
// Test: Missing key column in Record throws
// ============================================================================
static void test_missing_key_column() {
    auto path = make_temp_dir("misscol");
    cleanup(path);
    auto table = make_test_table(path);

    ColumnsIndex idx(table, {"SCAN"});

    Record bad_key;
    bad_key.set("WRONG_COLUMN", RecordValue(static_cast<std::int32_t>(1)));

    bool threw = false;
    try {
        (void)idx.get_row_numbers(bad_key);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "missing key column in Record throws");

    cleanup(path);
}

// ============================================================================
// Test: Empty key columns throws
// ============================================================================
static void test_empty_key_columns() {
    auto path = make_temp_dir("emptykey");
    cleanup(path);
    auto table = make_test_table(path);

    bool threw = false;
    try {
        ColumnsIndex idx(table, {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "empty key columns throws");

    cleanup(path);
}

// ============================================================================
// Test: rebuild after table modification
// ============================================================================
static void test_rebuild() {
    auto path = make_temp_dir("rebuild");
    cleanup(path);

    std::vector<TableColumnSpec> cols = {
        {.name = "KEY", .data_type = DataType::tp_int},
    };
    auto table = Table::create(path, cols, 2);
    table.write_scalar_cell("KEY", 0, CellValue(static_cast<std::int32_t>(10)));
    table.write_scalar_cell("KEY", 1, CellValue(static_cast<std::int32_t>(20)));
    table.flush();

    ColumnsIndex idx(table, {"KEY"});
    check(idx.unique_key_count() == 2, "initially 2 unique keys");

    // Add a row and rebuild
    table.add_row();
    table.write_scalar_cell("KEY", 2, CellValue(static_cast<std::int32_t>(30)));
    table.flush();
    idx.rebuild();
    check(idx.unique_key_count() == 3, "after rebuild, 3 unique keys");

    Record key;
    key.set("KEY", RecordValue(static_cast<std::int32_t>(30)));
    check(idx.contains(key), "new key found after rebuild");

    cleanup(path);
}

int main() {
    test_single_column_lookup();
    test_two_column_lookup();
    test_unique_key_count();
    test_contains();
    test_get_row_number_unique();
    test_missing_key_column();
    test_empty_key_columns();
    test_rebuild();

    std::cout << "columns_index_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
