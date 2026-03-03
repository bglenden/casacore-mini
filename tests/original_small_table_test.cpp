// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// original_small_table_test.cpp
// Standalone test: create small casacore tables using casacore-original,
// write scalar Int values, flush, reopen, and verify.
//
// Three test cases:
//   1. 1 column ("N"), 7 rows, values 0..6
//   2. 1 column ("N"), 1 row,  value 42
//   3. 1 column ("N"), 0 rows  (empty table)
//
// Compile manually against casacore-original, e.g.:
//   c++ -std=c++17 -I/path/to/casacore/include \
//       -L/path/to/casacore/lib -lcasa_tables -lcasa_casa \
//       -o original_small_table_test original_small_table_test.cpp
//
// Prints PASS or FAIL for each sub-test and returns 0 only if all pass.

#include <casacore/casa/OS/Path.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/TableUtil.h>

#include <casacore/casa/namespace.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Remove a table directory tree if it exists.
static void removeIfExists(const std::string& path) {
    if (fs::exists(path)) {
        fs::remove_all(path);
    }
}

// Create a table at `path` with a single ScalarColumn<Int>("N") and `nrow`
// rows, writing the values from `values`.  The table is flushed and closed
// before this function returns.
static void createTable(const std::string& path, const std::vector<Int>& values) {
    // Build description with one Int scalar column.
    TableDesc td("", "1", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("N", "test column"));

    // Create the table on disk.
    SetupNewTable setup(path, td, Table::New);
    rownr_t nrow = static_cast<rownr_t>(values.size());
    Table tab(setup, nrow);

    // Write values.
    ScalarColumn<Int> col(tab, "N");
    for (rownr_t r = 0; r < nrow; ++r) {
        col.put(r, values[r]);
    }

    // Flush (Table destructor also flushes, but be explicit).
    tab.flush();
}

// Reopen `path` read-only and verify that column "N" has exactly `expected`
// values.  Returns true on success.
static bool verifyTable(const std::string& path, const std::vector<Int>& expected) {
    Table tab(path, Table::Old);
    if (tab.nrow() != static_cast<rownr_t>(expected.size())) {
        std::cerr << "  row count mismatch: got " << tab.nrow() << ", expected " << expected.size()
                  << "\n";
        return false;
    }
    if (tab.nrow() == 0) {
        // Empty table -- nothing more to check.
        return true;
    }
    ScalarColumn<Int> col(tab, "N");
    for (rownr_t r = 0; r < tab.nrow(); ++r) {
        Int val = col(r);
        if (val != expected[r]) {
            std::cerr << "  value mismatch at row " << r << ": got " << val << ", expected "
                      << expected[r] << "\n";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Sub-tests
// ---------------------------------------------------------------------------

// Test 1: 1 column, 7 rows, values 0..6.
static bool test_7rows(const std::string& dir) {
    std::string path = dir + "/test_7rows.tab";
    removeIfExists(path);

    std::vector<Int> values = {0, 1, 2, 3, 4, 5, 6};
    createTable(path, values);
    bool ok = verifyTable(path, values);

    removeIfExists(path);
    return ok;
}

// Test 2: 1 column, 1 row, value 42.
static bool test_1row(const std::string& dir) {
    std::string path = dir + "/test_1row.tab";
    removeIfExists(path);

    std::vector<Int> values = {42};
    createTable(path, values);
    bool ok = verifyTable(path, values);

    removeIfExists(path);
    return ok;
}

// Test 3: 1 column, 0 rows (empty table).
static bool test_0rows(const std::string& dir) {
    std::string path = dir + "/test_0rows.tab";
    removeIfExists(path);

    std::vector<Int> values; // empty
    createTable(path, values);
    bool ok = verifyTable(path, values);

    removeIfExists(path);
    return ok;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // Use a temporary directory under /tmp for the test tables.
    std::string tmpdir = "/tmp/casacore_small_table_test";
    removeIfExists(tmpdir);
    fs::create_directories(tmpdir);

    int failures = 0;

    // --- Test 1: 7 rows ---
    std::cout << "Test 1 (1 col, 7 rows, values 0..6): " << std::flush;
    try {
        if (test_7rows(tmpdir)) {
            std::cout << "PASS\n";
        } else {
            std::cout << "FAIL\n";
            ++failures;
        }
    } catch (const std::exception& ex) {
        std::cout << "FAIL (exception: " << ex.what() << ")\n";
        ++failures;
    }

    // --- Test 2: 1 row ---
    std::cout << "Test 2 (1 col, 1 row,  value 42):    " << std::flush;
    try {
        if (test_1row(tmpdir)) {
            std::cout << "PASS\n";
        } else {
            std::cout << "FAIL\n";
            ++failures;
        }
    } catch (const std::exception& ex) {
        std::cout << "FAIL (exception: " << ex.what() << ")\n";
        ++failures;
    }

    // --- Test 3: 0 rows ---
    std::cout << "Test 3 (1 col, 0 rows, empty table): " << std::flush;
    try {
        if (test_0rows(tmpdir)) {
            std::cout << "PASS\n";
        } else {
            std::cout << "FAIL\n";
            ++failures;
        }
    } catch (const std::exception& ex) {
        std::cout << "FAIL (exception: " << ex.what() << ")\n";
        ++failures;
    }

    // Clean up the top-level tmp directory.
    removeIfExists(tmpdir);

    // Summary.
    if (failures == 0) {
        std::cout << "\nAll tests PASSED.\n";
    } else {
        std::cout << "\n" << failures << " test(s) FAILED.\n";
    }
    return failures == 0 ? 0 : 1;
}
