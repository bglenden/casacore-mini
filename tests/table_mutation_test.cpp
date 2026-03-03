// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

namespace {

fs::path temp_dir() {
    auto p = fs::temp_directory_path() / "casacore_mini_table_mutation_test";
    fs::create_directories(p);
    return p;
}

void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

// --- SSM tests ---

void test_add_row_ssm_scalar() {
    auto dir = temp_dir() / "add_row_ssm_scalar";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, "row id"},
        {"VALUE", DataType::tp_double, ColumnKind::scalar, {}, "value"},
    };

    auto table = Table::create(dir, cols, 3);
    table.write_scalar_cell("ID", 0, CellValue(std::int32_t{10}));
    table.write_scalar_cell("ID", 1, CellValue(std::int32_t{20}));
    table.write_scalar_cell("ID", 2, CellValue(std::int32_t{30}));
    table.write_scalar_cell("VALUE", 0, CellValue(1.5));
    table.write_scalar_cell("VALUE", 1, CellValue(2.5));
    table.write_scalar_cell("VALUE", 2, CellValue(3.5));
    table.flush();

    assert(table.nrow() == 3);

    // Add 2 rows.
    table.add_row(2);
    assert(table.nrow() == 5);

    // Original rows preserved.
    auto v0 = table.read_scalar_cell("ID", 0);
    assert(std::get<std::int32_t>(v0) == 10);
    auto v1 = table.read_scalar_cell("ID", 1);
    assert(std::get<std::int32_t>(v1) == 20);
    auto v2 = table.read_scalar_cell("ID", 2);
    assert(std::get<std::int32_t>(v2) == 30);

    // Write to new rows.
    table.write_scalar_cell("ID", 3, CellValue(std::int32_t{40}));
    table.write_scalar_cell("ID", 4, CellValue(std::int32_t{50}));
    table.write_scalar_cell("VALUE", 3, CellValue(4.5));
    table.write_scalar_cell("VALUE", 4, CellValue(5.5));
    table.flush();

    // Re-open and verify.
    auto t2 = Table::open(dir);
    assert(t2.nrow() == 5);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 3)) == 40);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 4)) == 50);
    assert(std::get<double>(t2.read_scalar_cell("VALUE", 4)) == 5.5);

    cleanup(dir);
}

void test_remove_row_ssm_scalar() {
    auto dir = temp_dir() / "remove_row_ssm_scalar";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"NAME", DataType::tp_string, ColumnKind::scalar, {}, "name"},
        {"NUM", DataType::tp_int, ColumnKind::scalar, {}, "number"},
    };

    auto table = Table::create(dir, cols, 4);
    table.write_scalar_cell("NAME", 0, CellValue(std::string("alpha")));
    table.write_scalar_cell("NAME", 1, CellValue(std::string("beta")));
    table.write_scalar_cell("NAME", 2, CellValue(std::string("gamma")));
    table.write_scalar_cell("NAME", 3, CellValue(std::string("delta")));
    table.write_scalar_cell("NUM", 0, CellValue(std::int32_t{1}));
    table.write_scalar_cell("NUM", 1, CellValue(std::int32_t{2}));
    table.write_scalar_cell("NUM", 2, CellValue(std::int32_t{3}));
    table.write_scalar_cell("NUM", 3, CellValue(std::int32_t{4}));
    table.flush();

    // Remove row 1 ("beta").
    table.remove_row(1);
    assert(table.nrow() == 3);

    // Verify remaining rows shifted correctly.
    table.flush();
    auto t2 = Table::open(dir);
    assert(t2.nrow() == 3);
    assert(std::get<std::string>(t2.read_scalar_cell("NAME", 0)) == "alpha");
    assert(std::get<std::string>(t2.read_scalar_cell("NAME", 1)) == "gamma");
    assert(std::get<std::string>(t2.read_scalar_cell("NAME", 2)) == "delta");
    assert(std::get<std::int32_t>(t2.read_scalar_cell("NUM", 0)) == 1);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("NUM", 1)) == 3);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("NUM", 2)) == 4);

    cleanup(dir);
}

void test_remove_first_row() {
    auto dir = temp_dir() / "remove_first_row";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 3);
    table.write_scalar_cell("X", 0, CellValue(std::int32_t{100}));
    table.write_scalar_cell("X", 1, CellValue(std::int32_t{200}));
    table.write_scalar_cell("X", 2, CellValue(std::int32_t{300}));
    table.flush();

    table.remove_row(0);
    assert(table.nrow() == 2);
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 2);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("X", 0)) == 200);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("X", 1)) == 300);

    cleanup(dir);
}

void test_remove_last_row() {
    auto dir = temp_dir() / "remove_last_row";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 3);
    table.write_scalar_cell("X", 0, CellValue(std::int32_t{100}));
    table.write_scalar_cell("X", 1, CellValue(std::int32_t{200}));
    table.write_scalar_cell("X", 2, CellValue(std::int32_t{300}));
    table.flush();

    table.remove_row(2);
    assert(table.nrow() == 2);
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 2);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("X", 0)) == 100);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("X", 1)) == 200);

    cleanup(dir);
}

void test_add_row_zero_does_nothing() {
    auto dir = temp_dir() / "add_zero";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 2);
    table.add_row(0);
    assert(table.nrow() == 2);

    cleanup(dir);
}

void test_remove_row_out_of_range() {
    auto dir = temp_dir() / "remove_oor";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 2);
    bool threw_oor = false;
    try {
        table.remove_row(5);
    } catch (const std::out_of_range&) {
        threw_oor = true;
    }
    (void)threw_oor;
    assert(threw_oor);

    cleanup(dir);
}

void test_add_row_read_only_throws() {
    auto dir = temp_dir() / "add_ro";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 2);
    table.flush();

    auto ro = Table::open(dir);
    bool threw_ro = false;
    try {
        ro.add_row(1);
    } catch (const std::runtime_error&) {
        threw_ro = true;
    }
    (void)threw_ro;
    assert(threw_ro);

    cleanup(dir);
}

// --- ISM tests ---

void test_add_row_ism() {
    auto dir = temp_dir() / "add_row_ism";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    TableCreateOptions opts;
    opts.sm_type = "IncrementalStMan";
    opts.sm_group = "IncrementalStMan";

    auto table = Table::create(dir, cols, 3, opts);
    table.write_scalar_cell("ID", 0, CellValue(std::int32_t{10}));
    table.write_scalar_cell("ID", 1, CellValue(std::int32_t{20}));
    table.write_scalar_cell("ID", 2, CellValue(std::int32_t{30}));
    table.flush();

    table.add_row(2);
    assert(table.nrow() == 5);

    table.write_scalar_cell("ID", 3, CellValue(std::int32_t{40}));
    table.write_scalar_cell("ID", 4, CellValue(std::int32_t{50}));
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 5);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 0)) == 10);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 3)) == 40);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 4)) == 50);

    cleanup(dir);
}

void test_remove_row_ism() {
    auto dir = temp_dir() / "remove_row_ism";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"ID", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    TableCreateOptions opts;
    opts.sm_type = "IncrementalStMan";
    opts.sm_group = "IncrementalStMan";

    auto table = Table::create(dir, cols, 3, opts);
    table.write_scalar_cell("ID", 0, CellValue(std::int32_t{10}));
    table.write_scalar_cell("ID", 1, CellValue(std::int32_t{20}));
    table.write_scalar_cell("ID", 2, CellValue(std::int32_t{30}));
    table.flush();

    table.remove_row(1);
    assert(table.nrow() == 2);
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 2);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 0)) == 10);
    assert(std::get<std::int32_t>(t2.read_scalar_cell("ID", 1)) == 30);

    cleanup(dir);
}

// --- Array column tests ---

void test_add_row_ssm_fixed_array() {
    auto dir = temp_dir() / "add_row_ssm_array";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"DATA", DataType::tp_float, ColumnKind::array, {2, 2}, "2x2 float"},
    };

    auto table = Table::create(dir, cols, 2);
    table.write_array_float_cell("DATA", 0, {1.0F, 2.0F, 3.0F, 4.0F});
    table.write_array_float_cell("DATA", 1, {5.0F, 6.0F, 7.0F, 8.0F});
    table.flush();

    table.add_row(1);
    assert(table.nrow() == 3);

    table.write_array_float_cell("DATA", 2, {9.0F, 10.0F, 11.0F, 12.0F});
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 3);
    auto d0 = t2.read_array_float_cell("DATA", 0);
    assert(d0.size() == 4);
    assert(d0[0] == 1.0F);
    auto d2 = t2.read_array_float_cell("DATA", 2);
    assert(d2.size() == 4);
    assert(d2[0] == 9.0F);

    cleanup(dir);
}

void test_remove_row_ssm_fixed_array() {
    auto dir = temp_dir() / "remove_row_ssm_array";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"DATA", DataType::tp_double, ColumnKind::array, {3}, "1D array"},
    };

    auto table = Table::create(dir, cols, 3);
    table.write_array_double_cell("DATA", 0, {1.0, 2.0, 3.0});
    table.write_array_double_cell("DATA", 1, {4.0, 5.0, 6.0});
    table.write_array_double_cell("DATA", 2, {7.0, 8.0, 9.0});
    table.flush();

    table.remove_row(1);
    assert(table.nrow() == 2);
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 2);
    auto d0 = t2.read_array_double_cell("DATA", 0);
    assert(d0[0] == 1.0);
    auto d1 = t2.read_array_double_cell("DATA", 1);
    assert(d1[0] == 7.0);

    cleanup(dir);
}

// --- TSM mutation tests (P12-W12) ---

void test_add_row_tsm() {
    auto dir = temp_dir() / "add_row_tsm";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"DATA", DataType::tp_float, ColumnKind::array, {4}, "float[4]"},
    };

    TableCreateOptions opts;
    opts.sm_type = "TiledColumnStMan";
    opts.sm_group = "TiledColumnStMan";
    opts.tile_shape = {4, 2};

    auto table = Table::create(dir, cols, 2, opts);
    table.write_array_float_cell("DATA", 0, {1.0F, 2.0F, 3.0F, 4.0F});
    table.write_array_float_cell("DATA", 1, {5.0F, 6.0F, 7.0F, 8.0F});
    table.flush();

    table.add_row(1);
    assert(table.nrow() == 3);

    table.write_array_float_cell("DATA", 2, {9.0F, 10.0F, 11.0F, 12.0F});
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 3);

    auto d0 = t2.read_array_float_cell("DATA", 0);
    assert(d0.size() == 4);
    assert(d0[0] == 1.0F);
    assert(d0[3] == 4.0F);

    auto d2 = t2.read_array_float_cell("DATA", 2);
    assert(d2.size() == 4);
    assert(d2[0] == 9.0F);
    assert(d2[3] == 12.0F);

    cleanup(dir);
}

void test_remove_row_tsm() {
    auto dir = temp_dir() / "remove_row_tsm";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"DATA", DataType::tp_double, ColumnKind::array, {2}, "double[2]"},
    };

    TableCreateOptions opts;
    opts.sm_type = "TiledColumnStMan";
    opts.sm_group = "TiledColumnStMan";
    opts.tile_shape = {2, 2};

    auto table = Table::create(dir, cols, 3, opts);
    table.write_array_double_cell("DATA", 0, {1.0, 2.0});
    table.write_array_double_cell("DATA", 1, {3.0, 4.0});
    table.write_array_double_cell("DATA", 2, {5.0, 6.0});
    table.flush();

    table.remove_row(1);
    assert(table.nrow() == 2);
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 2);

    auto d0 = t2.read_array_double_cell("DATA", 0);
    assert(d0.size() == 2);
    assert(d0[0] == 1.0);
    assert(d0[1] == 2.0);

    auto d1 = t2.read_array_double_cell("DATA", 1);
    assert(d1.size() == 2);
    assert(d1[0] == 5.0);
    assert(d1[1] == 6.0);

    cleanup(dir);
}

void test_add_remove_preserves_keywords() {
    auto dir = temp_dir() / "mutation_keywords";
    cleanup(dir);

    std::vector<TableColumnSpec> cols = {
        {"X", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 2);
    table.rw_keywords().set("version", RecordValue(std::int32_t{42}));
    table.write_scalar_cell("X", 0, CellValue(std::int32_t{1}));
    table.write_scalar_cell("X", 1, CellValue(std::int32_t{2}));
    table.flush();

    table.add_row(1);
    table.write_scalar_cell("X", 2, CellValue(std::int32_t{3}));
    table.flush();

    auto t2 = Table::open(dir);
    assert(t2.nrow() == 3);
    const auto* kw = t2.keywords().find("version");
    (void)kw;
    assert(kw != nullptr);
    assert(std::get<std::int32_t>(kw->storage()) == 42);

    cleanup(dir);
}

} // namespace

int main() {
    test_add_row_ssm_scalar();
    test_remove_row_ssm_scalar();
    test_remove_first_row();
    test_remove_last_row();
    test_add_row_zero_does_nothing();
    test_remove_row_out_of_range();
    test_add_row_read_only_throws();
    test_add_row_ism();
    test_remove_row_ism();
    test_add_row_ssm_fixed_array();
    test_remove_row_ssm_fixed_array();
    test_add_row_tsm();
    test_remove_row_tsm();
    test_add_remove_preserves_keywords();
    return 0;
}
