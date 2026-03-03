// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_directory.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::string test_data_dir() {
    return std::string(CASACORE_MINI_SOURCE_DIR) + "/data/corpus/fixtures";
}

bool test_read_directory_with_table_dat_only() {
    // table_dat_ttable2_v1 has only table.dat (no SM data files).
    const auto dir = test_data_dir() + "/table_dat_ttable2_v1";
    auto td = casacore_mini::read_table_directory(dir);

    assert(td.table_dat.table_version == 2);
    assert(td.table_dat.row_count == 10);
    assert(td.table_dat.table_desc.columns.size() == 9);
    assert(td.sm_files.empty()); // No .f files in fixture.

    std::cerr << "  PASS: test_read_directory_with_table_dat_only\n";
    return true;
}

bool test_write_and_read_roundtrip() {
    namespace fs = std::filesystem;

    // Read a fixture, write to temp, read back.
    const auto src = test_data_dir() + "/table_dat_ttable2_v1";
    auto original = casacore_mini::read_table_directory(src);

    const auto tmp = fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_td_roundtrip";
    fs::remove_all(tmp);

    casacore_mini::write_table_directory(tmp.string(), original.table_dat);
    auto roundtripped = casacore_mini::read_table_directory(tmp.string());

    // Version may differ (writer produces v2).
    assert(roundtripped.table_dat.row_count == original.table_dat.row_count);
    assert(roundtripped.table_dat.table_desc.columns.size() ==
           original.table_dat.table_desc.columns.size());

    for (std::size_t i = 0; i < original.table_dat.table_desc.columns.size(); ++i) {
        assert(roundtripped.table_dat.table_desc.columns[i].name ==
               original.table_dat.table_desc.columns[i].name);
    }

    fs::remove_all(tmp);
    std::cerr << "  PASS: test_write_and_read_roundtrip\n";
    return true;
}

bool test_compare_identical() {
    const auto dir = test_data_dir() + "/table_dat_ttable2_v1";
    auto a = casacore_mini::read_table_directory(dir);
    auto b = casacore_mini::read_table_directory(dir);

    auto diff = casacore_mini::compare_table_directories(a, b);
    assert(diff.empty());

    std::cerr << "  PASS: test_compare_identical\n";
    return true;
}

bool test_missing_table_dat() {
    namespace fs = std::filesystem;
    const auto tmp = fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_td_missing";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    try {
        static_cast<void>(casacore_mini::read_table_directory(tmp.string()));
        assert(false && "expected exception for missing table.dat");
    } catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch)
        // expected
    }

    fs::remove_all(tmp);
    std::cerr << "  PASS: test_missing_table_dat\n";
    return true;
}

} // namespace

int main() noexcept {
    try {
        std::cerr << "table_directory_test:\n";
        if (!test_read_directory_with_table_dat_only()) {
            return EXIT_FAILURE;
        }
        if (!test_write_and_read_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_compare_identical()) {
            return EXIT_FAILURE;
        }
        if (!test_missing_table_dat()) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_directory_test threw: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
