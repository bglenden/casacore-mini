// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file table_lock_test.cpp
/// @brief P12-W10 tests for Table lock model and lock-aware utilities.

#include "casacore_mini/table.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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
           ("tbl_lock_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p))
        fs::remove_all(p);
}

static Table make_test_table(const fs::path& path) {
    std::vector<TableColumnSpec> cols = {
        {.name = "COL_A", .data_type = DataType::tp_int, .shape = {}},
        {.name = "COL_B", .data_type = DataType::tp_double, .shape = {}},
    };
    return Table::create(path, cols, 3);
}

// ============================================================================
// Test: Lock/unlock basic flow
// ============================================================================
static void test_lock_unlock() {
    auto path = make_temp_dir("lock");
    cleanup(path);
    auto table = make_test_table(path);

    check(!table.has_lock(), "initially no lock held");
    check(!table.is_locked(), "lock file initially empty");

    table.lock(true);
    check(table.has_lock(), "has_lock after lock()");
    check(table.is_locked(), "is_locked after lock()");

    table.unlock();
    check(!table.has_lock(), "no lock after unlock()");
    check(!table.is_locked(), "not is_locked after unlock()");

    std::cout << "  lock/unlock basic... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: Read vs write lock
// ============================================================================
static void test_read_write_lock() {
    auto path = make_temp_dir("rw_lock");
    cleanup(path);
    auto table = make_test_table(path);

    table.lock(false);
    check(table.has_lock(), "has read lock");

    // Read the lock file content
    {
        std::ifstream in(path / "table.lock");
        std::string line;
        std::getline(in, line);
        check(line == "read", "lock file says 'read'");
    }

    table.unlock();
    table.lock(true);

    {
        std::ifstream in(path / "table.lock");
        std::string line;
        std::getline(in, line);
        check(line == "write", "lock file says 'write'");
    }

    table.unlock();
    std::cout << "  read/write lock... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: Lock mode accessor
// ============================================================================
static void test_lock_mode() {
    auto path = make_temp_dir("mode");
    cleanup(path);
    auto table = make_test_table(path);

    check(table.lock_mode() == TableLockMode::NoLock, "default lock mode is NoLock");

    table.set_lock_mode(TableLockMode::AutoLock);
    check(table.lock_mode() == TableLockMode::AutoLock, "set to AutoLock");

    table.set_lock_mode(TableLockMode::PermanentLock);
    check(table.lock_mode() == TableLockMode::PermanentLock, "set to PermanentLock");

    table.set_lock_mode(TableLockMode::UserLock);
    check(table.lock_mode() == TableLockMode::UserLock, "set to UserLock");

    std::cout << "  lock mode accessor... PASS\n";
    cleanup(path);
}

// ============================================================================
// Test: drop_table locked vs unlocked
// ============================================================================
static void test_drop_table() {
    {
        auto path = make_temp_dir("drop_ok");
        cleanup(path);
        make_test_table(path);
        check(fs::exists(path), "table exists before drop");
        auto ok = Table::drop_table(path);
        check(ok, "drop_table returns true");
        check(!fs::exists(path), "table removed after drop");
        std::cout << "  drop_table (unlocked)... PASS\n";
    }

    {
        auto path = make_temp_dir("drop_locked");
        cleanup(path);
        auto table = make_test_table(path);
        table.lock(true);

        // Drop should throw because table is locked
        bool threw = false;
        try {
            (void)Table::drop_table(path, false);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "drop_table throws when locked");

        // Force drop should work
        auto ok = Table::drop_table(path, true);
        check(ok, "force drop returns true");
        check(!fs::exists(path), "table removed after force drop");
        std::cout << "  drop_table (locked/force)... PASS\n";
    }

    {
        auto path = make_temp_dir("drop_nonexist");
        cleanup(path);
        auto ok = Table::drop_table(path);
        check(!ok, "drop_table returns false for nonexistent path");
        std::cout << "  drop_table (nonexistent)... PASS\n";
    }
}

int main() {
    try {
        test_lock_unlock();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try {
        test_read_write_lock();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try {
        test_lock_mode();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION: " << e.what() << "\n";
    }
    try {
        test_drop_table();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION: " << e.what() << "\n";
    }

    std::cout << "table_lock_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
