/// @file table_iter_test.cpp
/// @brief P12-W11 tests for TableIterator (group-by iteration).

#include "casacore_mini/table_iterator.hpp"
#include "casacore_mini/table.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

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
           ("tbl_iter_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) fs::remove_all(p);
}

/// Create a table with FIELD_ID and SCAN columns, 8 rows:
///  row 0: FIELD=0, SCAN=1
///  row 1: FIELD=1, SCAN=1
///  row 2: FIELD=0, SCAN=2
///  row 3: FIELD=0, SCAN=1
///  row 4: FIELD=1, SCAN=2
///  row 5: FIELD=1, SCAN=1
///  row 6: FIELD=0, SCAN=2
///  row 7: FIELD=1, SCAN=2
static Table make_test_table(const fs::path& path) {
    std::vector<TableColumnSpec> cols = {
        {.name = "FIELD_ID", .data_type = DataType::tp_int},
        {.name = "SCAN", .data_type = DataType::tp_int},
    };
    auto table = Table::create(path, cols, 8);
    std::int32_t fields[] = {0, 1, 0, 0, 1, 1, 0, 1};
    std::int32_t scans[]  = {1, 1, 2, 1, 2, 1, 2, 2};
    for (int i = 0; i < 8; ++i) {
        auto r = static_cast<std::uint64_t>(i);
        table.write_scalar_cell("FIELD_ID", r, CellValue(fields[i]));
        table.write_scalar_cell("SCAN", r, CellValue(scans[i]));
    }
    table.flush();
    return table;
}

// ============================================================================
// Test: Single-column grouping
// ============================================================================
static void test_single_column_grouping() {
    auto path = make_temp_dir("single");
    cleanup(path);
    auto table = make_test_table(path);

    TableIterator iter(table, {"FIELD_ID"});
    check(!iter.at_end(), "not at end before start");

    int group_count = 0;
    std::uint64_t total_rows = 0;
    while (iter.next()) {
        ++group_count;
        total_rows += iter.current().nrow();
    }
    check(group_count == 2, "2 groups for FIELD_ID (0 and 1)");
    check(total_rows == 8, "all 8 rows accounted for");
    check(iter.at_end(), "at end after exhaustion");

    cleanup(path);
}

// ============================================================================
// Test: Two-column grouping
// ============================================================================
static void test_two_column_grouping() {
    auto path = make_temp_dir("two");
    cleanup(path);
    auto table = make_test_table(path);

    // Groups: (0,1), (0,2), (1,1), (1,2)
    TableIterator iter(table, {"FIELD_ID", "SCAN"});

    int group_count = 0;
    std::vector<std::uint64_t> group_sizes;
    while (iter.next()) {
        ++group_count;
        group_sizes.push_back(iter.current().nrow());
    }
    check(group_count == 4, "4 groups for (FIELD_ID, SCAN)");

    // Sort group sizes to check distribution (order depends on sort)
    std::sort(group_sizes.begin(), group_sizes.end());
    // Expected: (0,1)→2 rows, (0,2)→2 rows, (1,1)→2 rows, (1,2)→2 rows
    check(group_sizes[0] == 2, "smallest group has 2 rows");
    check(group_sizes[3] == 2, "largest group has 2 rows");

    cleanup(path);
}

// ============================================================================
// Test: Group content verification
// ============================================================================
static void test_group_content() {
    auto path = make_temp_dir("content");
    cleanup(path);
    auto table = make_test_table(path);

    TableIterator iter(table, {"FIELD_ID"});

    // First group should have all rows with the smallest FIELD_ID (0)
    check(iter.next(), "first next() succeeds");
    auto& g1 = iter.current();
    auto first_field = std::get<std::int32_t>(g1.read_scalar_cell("FIELD_ID", 0));
    check(first_field == 0, "first group is FIELD_ID=0");

    // All rows in group should have same FIELD_ID
    for (std::uint64_t r = 0; r < g1.nrow(); ++r) {
        auto f = std::get<std::int32_t>(g1.read_scalar_cell("FIELD_ID", r));
        check(f == 0, "all rows in group 1 have FIELD_ID=0");
    }

    // Second group should be FIELD_ID=1
    check(iter.next(), "second next() succeeds");
    auto& g2 = iter.current();
    auto second_field = std::get<std::int32_t>(g2.read_scalar_cell("FIELD_ID", 0));
    check(second_field == 1, "second group is FIELD_ID=1");

    check(!iter.next(), "no more groups");

    cleanup(path);
}

// ============================================================================
// Test: Reset
// ============================================================================
static void test_reset() {
    auto path = make_temp_dir("reset");
    cleanup(path);
    auto table = make_test_table(path);

    TableIterator iter(table, {"FIELD_ID"});

    int count1 = 0;
    while (iter.next()) ++count1;

    iter.reset();
    check(!iter.at_end(), "after reset, not at end");

    int count2 = 0;
    while (iter.next()) ++count2;
    check(count1 == count2, "same group count after reset");

    cleanup(path);
}

// ============================================================================
// Test: Empty key throws
// ============================================================================
static void test_empty_key_throws() {
    auto path = make_temp_dir("nokey");
    cleanup(path);
    auto table = make_test_table(path);

    bool threw = false;
    try {
        TableIterator iter(table, {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "empty key columns throws");

    cleanup(path);
}

// ============================================================================
// Test: current() before next() throws
// ============================================================================
static void test_current_before_next_throws() {
    auto path = make_temp_dir("noinit");
    cleanup(path);
    auto table = make_test_table(path);

    TableIterator iter(table, {"FIELD_ID"});

    bool threw = false;
    try {
        (void)iter.current();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "current() before next() throws");

    cleanup(path);
}

int main() {
    test_single_column_grouping();
    test_two_column_grouping();
    test_group_content();
    test_reset();
    test_empty_key_throws();
    test_current_before_next_throws();

    std::cout << "table_iter_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
