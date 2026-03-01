#include "casacore_mini/table.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

static fs::path make_temp_dir(const std::string& suffix) {
    return fs::temp_directory_path() /
           ("tapi_ext_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Helper: create a simple 3-column scalar table with given nrows.
static Table make_test_table(const fs::path& path, std::uint64_t nrows) {
    cleanup(path);
    std::vector<TableColumnSpec> columns = {
        {.name = "ID",
         .data_type = DataType::tp_int,
         .kind = ColumnKind::scalar,
         .shape = {},
         .comment = "row id"},
        {.name = "VALUE",
         .data_type = DataType::tp_double,
         .kind = ColumnKind::scalar,
         .shape = {},
         .comment = "value"},
        {.name = "LABEL",
         .data_type = DataType::tp_string,
         .kind = ColumnKind::scalar,
         .shape = {},
         .comment = "label"},
    };
    auto table = Table::create(path, columns, nrows);
    for (std::uint64_t r = 0; r < nrows; ++r) {
        table.write_scalar_cell("ID", r, CellValue{static_cast<std::int32_t>(r * 10)});
        table.write_scalar_cell("VALUE", r, CellValue{static_cast<double>(r) * 1.5});
        table.write_scalar_cell("LABEL", r, CellValue{std::string("row_") + std::to_string(r)});
    }
    return table;
}

/// Helper: create a simple ISM-backed scalar table with one Int column.
static Table make_ism_test_table(const fs::path& path, std::uint64_t nrows) {
    cleanup(path);
    std::vector<TableColumnSpec> columns = {
        {.name = "ID",
         .data_type = DataType::tp_int,
         .kind = ColumnKind::scalar,
         .shape = {},
         .comment = "row id"},
    };
    TableCreateOptions opts;
    opts.sm_type = "IncrementalStMan";
    opts.sm_group = "ISMData";
    auto table = Table::create(path, columns, nrows, opts);
    for (std::uint64_t r = 0; r < nrows; ++r) {
        table.write_scalar_cell("ID", r, CellValue{static_cast<std::int32_t>(r * 10)});
    }
    return table;
}

/// Helper: create a simple TSM-backed fixed-shape Int array table.
static Table make_tsm_test_table(const fs::path& path, std::uint64_t nrows) {
    cleanup(path);
    std::vector<TableColumnSpec> columns = {
        {.name = "PIX",
         .data_type = DataType::tp_int,
         .kind = ColumnKind::array,
         .shape = {2},
         .comment = "pixels"},
    };
    TableCreateOptions opts;
    opts.sm_type = "TiledCellStMan";
    opts.sm_group = "TiledCell";
    auto table = Table::create(path, columns, nrows, opts);
    for (std::uint64_t r = 0; r < nrows; ++r) {
        table.write_array_int_cell(
            "PIX", r,
            {static_cast<std::int32_t>(r * 10 + 1), static_cast<std::int32_t>(r * 10 + 2)});
    }
    return table;
}

// ---------------------------------------------------------------------------
// Test: add_rows
// ---------------------------------------------------------------------------
static void test_add_rows_basic() {
    std::cout << "  add_rows basic... ";
    auto path = make_temp_dir("add_rows");
    auto table = make_test_table(path, 3);

    check(table.nrow() == 3, "initial nrow == 3");

    table.add_rows(2);
    check(table.nrow() == 5, "nrow after add_rows(2) == 5");

    // Write to new rows.
    table.write_scalar_cell("ID", 3, CellValue{std::int32_t{30}});
    table.write_scalar_cell("ID", 4, CellValue{std::int32_t{40}});

    // Flush and reopen to verify persistence.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.nrow() == 5, "reopened nrow == 5");

    // Old data should be preserved.
    auto rv0 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 0));
    check(rv0 == 0, "row 0 ID preserved");

    auto rv2 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 2));
    check(rv2 == 20, "row 2 ID preserved");

    // New data should be present.
    auto rv3 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 3));
    check(rv3 == 30, "row 3 value");

    auto rv4 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 4));
    check(rv4 == 40, "row 4 value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_add_rows_zero() {
    std::cout << "  add_rows(0) is no-op... ";
    auto path = make_temp_dir("add_rows0");
    auto table = make_test_table(path, 2);
    table.add_rows(0);
    check(table.nrow() == 2, "nrow unchanged after add_rows(0)");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_add_rows_not_writable() {
    std::cout << "  add_rows on read-only throws... ";
    auto path = make_temp_dir("add_rows_ro");
    {
        auto table = make_test_table(path, 1);
        table.flush();
    }
    auto table = Table::open(path);
    bool threw = false;
    try {
        table.add_rows(1);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "add_rows threw on read-only");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: remove_rows
// ---------------------------------------------------------------------------
static void test_remove_rows_basic() {
    std::cout << "  remove_rows basic... ";
    auto path = make_temp_dir("rm_rows");
    {
        auto table = make_test_table(path, 5);
        table.flush();
    }

    {
        auto table = Table::open_rw(path);

        // Remove rows 1 and 3 (0-indexed). Keep rows 0, 2, 4.
        table.remove_rows({1, 3});
        check(table.nrow() == 3, "nrow after removing 2 rows");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.nrow() == 3, "reopened nrow == 3");

    // Row 0 was original row 0: ID=0
    auto id0 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 0));
    check(id0 == 0, "row 0 is original row 0");

    // Row 1 was original row 2: ID=20
    auto id1 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 1));
    check(id1 == 20, "row 1 is original row 2");

    // Row 2 was original row 4: ID=40
    auto id2 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 2));
    check(id2 == 40, "row 2 is original row 4");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_rows_empty() {
    std::cout << "  remove_rows empty list is no-op... ";
    auto path = make_temp_dir("rm_rows_empty");
    auto table = make_test_table(path, 3);
    table.remove_rows({});
    check(table.nrow() == 3, "nrow unchanged");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_rows_out_of_range() {
    std::cout << "  remove_rows out-of-range throws... ";
    auto path = make_temp_dir("rm_rows_oor");
    auto table = make_test_table(path, 3);
    bool threw = false;
    try {
        table.remove_rows({5});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "remove_rows threw on out-of-range");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_rows_all() {
    std::cout << "  remove_rows all rows... ";
    auto path = make_temp_dir("rm_rows_all");
    auto table = make_test_table(path, 3);
    table.remove_rows({0, 1, 2});
    check(table.nrow() == 0, "nrow == 0 after removing all");
    table.flush();
    auto t2 = Table::open(path);
    check(t2.nrow() == 0, "reopened nrow == 0");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: add_column
// ---------------------------------------------------------------------------
static void test_add_column_basic() {
    std::cout << "  add_column basic... ";
    auto path = make_temp_dir("add_col");
    auto table = make_test_table(path, 3);
    check(table.ncolumn() == 3, "initial ncolumn == 3");

    TableColumnSpec spec{
        .name = "EXTRA",
        .data_type = DataType::tp_float,
        .kind = ColumnKind::scalar,
        .shape = {},
        .comment = "extra column",
    };
    table.add_column(spec);
    check(table.ncolumn() == 4, "ncolumn after add_column == 4");

    // Write to the new column.
    table.write_scalar_cell("EXTRA", 0, CellValue{3.14F});

    // Flush and reopen.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.ncolumn() == 4, "reopened ncolumn == 4");

    // Check original data preserved.
    auto v0 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 0));
    check(v0 == 0, "original ID preserved after add_column");

    // Check new column value.
    auto rv = std::get<float>(t2.read_scalar_cell("EXTRA", 0));
    check(std::fabs(rv - 3.14F) < 1e-5F, "reopened new column value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_add_column_duplicate() {
    std::cout << "  add_column duplicate throws... ";
    auto path = make_temp_dir("add_col_dup");
    auto table = make_test_table(path, 1);

    TableColumnSpec spec{
        .name = "ID",
        .data_type = DataType::tp_int,
        .kind = ColumnKind::scalar,
        .shape = {},
        .comment = {},
    };
    bool threw = false;
    try {
        table.add_column(spec);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "add_column threw on duplicate");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: remove_column
// ---------------------------------------------------------------------------
static void test_remove_column_basic() {
    std::cout << "  remove_column basic... ";
    auto path = make_temp_dir("rm_col");
    auto table = make_test_table(path, 3);
    check(table.ncolumn() == 3, "initial ncolumn == 3");

    table.remove_column("VALUE");
    check(table.ncolumn() == 2, "ncolumn after remove == 2");

    // Remaining columns should be ID and LABEL.
    const auto& cols = table.columns();
    check(cols[0].name == "ID", "first column is ID");
    check(cols[1].name == "LABEL", "second column is LABEL");

    // Flush and reopen to verify persistence and data.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.ncolumn() == 2, "reopened ncolumn == 2");

    // Data should still be accessible after reopen.
    auto id = std::get<std::int32_t>(t2.read_scalar_cell("ID", 1));
    check(id == 10, "ID data preserved");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_column_not_found() {
    std::cout << "  remove_column not found throws... ";
    auto path = make_temp_dir("rm_col_nf");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.remove_column("NONEXISTENT");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "remove_column threw on missing column");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: rename_column
// ---------------------------------------------------------------------------
static void test_rename_column_basic() {
    std::cout << "  rename_column basic... ";
    auto path = make_temp_dir("ren_col");
    auto table = make_test_table(path, 3);

    table.rename_column("VALUE", "AMOUNT");
    check(table.ncolumn() == 3, "ncolumn unchanged");

    // Old name gone, new name exists (in metadata).
    check(table.find_column_desc("VALUE") == nullptr, "old name gone");
    check(table.find_column_desc("AMOUNT") != nullptr, "new name exists");

    // Write via new name.
    table.write_scalar_cell("AMOUNT", 0, CellValue{99.9});

    // Flush and reopen to verify persistence.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.find_column_desc("AMOUNT") != nullptr, "reopened has new name");
    check(t2.find_column_desc("VALUE") == nullptr, "reopened lost old name");

    // Data should be accessible via new name after reopen.
    auto v = std::get<double>(t2.read_scalar_cell("AMOUNT", 1));
    check(std::fabs(v - 1.5) < 1e-10, "data accessible via new name");

    auto v2 = std::get<double>(t2.read_scalar_cell("AMOUNT", 0));
    check(std::fabs(v2 - 99.9) < 1e-10, "write via new name persisted");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_rename_column_not_found() {
    std::cout << "  rename_column not found throws... ";
    auto path = make_temp_dir("ren_col_nf");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.rename_column("NOPE", "ALSO_NOPE");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "rename_column threw on missing source");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_rename_column_duplicate_target() {
    std::cout << "  rename_column duplicate target throws... ";
    auto path = make_temp_dir("ren_col_dup");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.rename_column("ID", "LABEL");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "rename_column threw on duplicate target");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: set_keyword / remove_keyword
// ---------------------------------------------------------------------------
static void test_set_keyword() {
    std::cout << "  set_keyword... ";
    auto path = make_temp_dir("set_kw");
    auto table = make_test_table(path, 1);

    table.set_keyword("VERSION", RecordValue(std::int32_t{42}));
    table.set_keyword("AUTHOR", RecordValue(std::string("test")));

    const auto& kw = table.keywords();
    check(kw.size() == 2, "2 keywords set");

    auto* vp = kw.find("VERSION");
    check(vp != nullptr, "VERSION found");
    check(std::get<std::int32_t>(vp->storage()) == 42, "VERSION == 42");

    auto* ap = kw.find("AUTHOR");
    check(ap != nullptr, "AUTHOR found");
    check(std::get<std::string>(ap->storage()) == "test", "AUTHOR == test");

    // Overwrite.
    table.set_keyword("VERSION", RecordValue(std::int32_t{43}));
    check(std::get<std::int32_t>(kw.find("VERSION")->storage()) == 43, "VERSION updated to 43");

    // Persist.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.keywords().size() == 2, "reopened has 2 keywords");
    check(std::get<std::int32_t>(t2.keywords().find("VERSION")->storage()) == 43,
          "reopened VERSION == 43");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_keyword() {
    std::cout << "  remove_keyword... ";
    auto path = make_temp_dir("rm_kw");
    auto table = make_test_table(path, 1);

    table.set_keyword("A", RecordValue(std::int32_t{1}));
    table.set_keyword("B", RecordValue(std::int32_t{2}));
    check(table.keywords().size() == 2, "2 keywords before remove");

    table.remove_keyword("A");
    check(table.keywords().size() == 1, "1 keyword after remove");
    check(table.keywords().find("A") == nullptr, "A gone");
    check(table.keywords().find("B") != nullptr, "B remains");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_remove_keyword_not_found() {
    std::cout << "  remove_keyword not found throws... ";
    auto path = make_temp_dir("rm_kw_nf");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.remove_keyword("NOPE");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "remove_keyword threw on missing key");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_set_keyword_not_writable() {
    std::cout << "  set_keyword on read-only throws... ";
    auto path = make_temp_dir("set_kw_ro");
    {
        auto table = make_test_table(path, 1);
        table.flush();
    }
    auto table = Table::open(path);
    bool threw = false;
    try {
        table.set_keyword("X", RecordValue(std::int32_t{1}));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "set_keyword threw on read-only");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: copy_column
// ---------------------------------------------------------------------------
static void test_copy_column_scalar() {
    std::cout << "  copy_column scalar... ";
    auto path = make_temp_dir("copy_col");
    auto table = make_test_table(path, 3);

    table.copy_column("ID", "ID_COPY");
    check(table.ncolumn() == 4, "ncolumn after copy == 4");
    check(table.find_column_desc("ID_COPY") != nullptr, "copy column exists");

    // Flush and reopen.
    table.flush();
    auto t2 = Table::open(path);
    check(t2.ncolumn() == 4, "reopened ncolumn == 4");

    // Verify copy matches original.
    for (std::uint64_t r = 0; r < 3; ++r) {
        auto orig = std::get<std::int32_t>(t2.read_scalar_cell("ID", r));
        auto copy = std::get<std::int32_t>(t2.read_scalar_cell("ID_COPY", r));
        check(orig == copy, "copy matches original");
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_copy_column_not_found() {
    std::cout << "  copy_column source not found throws... ";
    auto path = make_temp_dir("copy_col_nf");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.copy_column("NOPE", "NOPE2");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "copy_column threw on missing source");
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_copy_column_dst_exists() {
    std::cout << "  copy_column dst exists throws... ";
    auto path = make_temp_dir("copy_col_dup");
    auto table = make_test_table(path, 1);
    bool threw = false;
    try {
        table.copy_column("ID", "LABEL");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "copy_column threw on existing dst");
    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: drop
// ---------------------------------------------------------------------------
static void test_drop() {
    std::cout << "  drop... ";
    auto path = make_temp_dir("drop");
    {
        auto table = make_test_table(path, 2);
        table.flush();
    }
    check(fs::exists(path), "table dir exists before drop");
    Table::drop(path);
    check(!fs::exists(path), "table dir gone after drop");
    std::cout << "PASS\n";
}

static void test_drop_nonexistent() {
    std::cout << "  drop nonexistent throws... ";
    auto path = make_temp_dir("drop_nx");
    cleanup(path); // ensure it does not exist
    bool threw = false;
    try {
        Table::drop(path);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "drop threw on nonexistent path");
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: combined operations
// ---------------------------------------------------------------------------
static void test_add_rows_then_remove() {
    std::cout << "  add rows then remove... ";
    auto path = make_temp_dir("add_rm");
    {
        auto table = make_test_table(path, 3);
        table.add_rows(2);
        check(table.nrow() == 5, "nrow after add == 5");

        table.write_scalar_cell("ID", 3, CellValue{std::int32_t{30}});
        table.write_scalar_cell("ID", 4, CellValue{std::int32_t{40}});
        table.flush();
    }

    {
        auto table = Table::open_rw(path);
        check(table.nrow() == 5, "reopened nrow == 5 before remove");

        table.remove_rows({0, 2});
        check(table.nrow() == 3, "nrow after remove == 3");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.nrow() == 3, "reopened nrow == 3");

    // Remaining rows: original 1 (ID=10), 3 (ID=30), 4 (ID=40)
    auto id0 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 0));
    check(id0 == 10, "first remaining row");

    auto id1 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 1));
    check(id1 == 30, "second remaining row");

    auto id2 = std::get<std::int32_t>(t2.read_scalar_cell("ID", 2));
    check(id2 == 40, "third remaining row");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_add_column_rename_remove() {
    std::cout << "  add column, rename, then remove... ";
    auto path = make_temp_dir("col_ops");
    {
        auto table = make_test_table(path, 2);

        // Add column.
        TableColumnSpec spec{
            .name = "TEMP",
            .data_type = DataType::tp_int,
            .kind = ColumnKind::scalar,
            .shape = {},
            .comment = {},
        };
        table.add_column(spec);
        check(table.ncolumn() == 4, "ncolumn after add");

        // Write to it.
        table.write_scalar_cell("TEMP", 0, CellValue{std::int32_t{100}});

        // Rename it.
        table.rename_column("TEMP", "RENAMED");
        check(table.find_column_desc("TEMP") == nullptr, "old name gone");
        check(table.find_column_desc("RENAMED") != nullptr, "new name exists");

        // Remove it.
        table.remove_column("RENAMED");
        check(table.ncolumn() == 3, "back to 3 columns");

        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.ncolumn() == 3, "reopened 3 columns");
    check(t2.find_column_desc("RENAMED") == nullptr, "RENAMED gone after reopen");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_keyword_persist_round_trip() {
    std::cout << "  keyword persist round-trip... ";
    auto path = make_temp_dir("kw_persist");
    {
        auto table = make_test_table(path, 1);
        table.set_keyword("PI", RecordValue(3.14159));
        table.set_keyword("NAME", RecordValue(std::string("test_table")));
        table.flush();
    }
    {
        auto table = Table::open(path);
        const auto& kw = table.keywords();
        check(kw.size() == 2, "2 keywords persisted");
        check(std::fabs(std::get<double>(kw.find("PI")->storage()) - 3.14159) < 1e-10,
              "PI value correct");
        check(std::get<std::string>(kw.find("NAME")->storage()) == "test_table",
              "NAME value correct");
    }
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_copy_column_double() {
    std::cout << "  copy_column double type... ";
    auto path = make_temp_dir("copy_dbl");
    {
        auto table = make_test_table(path, 3);
        table.copy_column("VALUE", "VALUE_COPY");
        table.flush();
    }
    auto t2 = Table::open(path);
    for (std::uint64_t r = 0; r < 3; ++r) {
        auto orig = std::get<double>(t2.read_scalar_cell("VALUE", r));
        auto copy = std::get<double>(t2.read_scalar_cell("VALUE_COPY", r));
        check(std::fabs(orig - copy) < 1e-10, "double copy matches");
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_copy_column_string() {
    std::cout << "  copy_column string type... ";
    auto path = make_temp_dir("copy_str");
    {
        auto table = make_test_table(path, 3);
        table.copy_column("LABEL", "LABEL_COPY");
        table.flush();
    }
    auto t2 = Table::open(path);
    for (std::uint64_t r = 0; r < 3; ++r) {
        auto orig = std::get<std::string>(t2.read_scalar_cell("LABEL", r));
        auto copy = std::get<std::string>(t2.read_scalar_cell("LABEL_COPY", r));
        check(orig == copy, "string copy matches");
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_ism_open_rw_write() {
    std::cout << "  ISM open_rw write... ";
    auto path = make_temp_dir("ism_open_rw");
    {
        auto table = make_ism_test_table(path, 2);
        table.flush();
    }
    {
        auto table = Table::open_rw(path);
        table.write_scalar_cell("ID", 1, CellValue{std::int32_t{77}});
        table.flush();
    }
    {
        auto table = Table::open(path);
        auto v = std::get<std::int32_t>(table.read_scalar_cell("ID", 1));
        check(v == 77, "ISM open_rw write persisted");
    }
    cleanup(path);
    std::cout << "PASS\n";
}

static void test_ism_add_rows_basic() {
    std::cout << "  ISM add_rows basic... ";
    auto path = make_temp_dir("ism_add_rows");
    auto table = make_ism_test_table(path, 3);
    table.add_rows(2);
    check(table.nrow() == 5, "ISM nrow after add_rows(2) == 5");
    table.write_scalar_cell("ID", 3, CellValue{std::int32_t{30}});
    table.write_scalar_cell("ID", 4, CellValue{std::int32_t{40}});
    table.flush();

    auto t2 = Table::open(path);
    check(t2.nrow() == 5, "ISM reopened nrow == 5");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 0)) == 0, "ISM row 0 preserved");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 2)) == 20, "ISM row 2 preserved");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 3)) == 30, "ISM row 3 value");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 4)) == 40, "ISM row 4 value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_ism_remove_rows_basic() {
    std::cout << "  ISM remove_rows basic... ";
    auto path = make_temp_dir("ism_rm_rows");
    {
        auto table = make_ism_test_table(path, 5);
        table.flush();
    }
    {
        auto table = Table::open_rw(path);
        table.remove_rows({1, 3});
        check(table.nrow() == 3, "ISM nrow after removing 2 rows");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.nrow() == 3, "ISM reopened nrow == 3");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 0)) == 0, "ISM row 0 after compact");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 1)) == 20, "ISM row 1 after compact");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 2)) == 40, "ISM row 2 after compact");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_ism_add_column_basic() {
    std::cout << "  ISM add_column basic... ";
    auto path = make_temp_dir("ism_add_col");
    auto table = make_ism_test_table(path, 3);
    table.add_column(TableColumnSpec{
        .name = "EXTRA",
        .data_type = DataType::tp_int,
        .kind = ColumnKind::scalar,
        .shape = {},
        .comment = "extra column",
    });
    check(table.ncolumn() == 2, "ISM ncolumn after add_column == 2");
    table.write_scalar_cell("EXTRA", 0, CellValue{std::int32_t{7}});
    table.write_scalar_cell("EXTRA", 2, CellValue{std::int32_t{9}});
    table.flush();

    auto t2 = Table::open(path);
    check(t2.ncolumn() == 2, "ISM reopened ncolumn == 2");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 2)) == 20, "ISM original column preserved");
    check(std::get<std::int32_t>(t2.read_scalar_cell("EXTRA", 0)) == 7, "ISM new column row 0 value");
    check(std::get<std::int32_t>(t2.read_scalar_cell("EXTRA", 1)) == 0, "ISM new column default row 1");
    check(std::get<std::int32_t>(t2.read_scalar_cell("EXTRA", 2)) == 9, "ISM new column row 2 value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_ism_remove_column_basic() {
    std::cout << "  ISM remove_column basic... ";
    auto path = make_temp_dir("ism_rm_col");
    {
        auto table = make_ism_test_table(path, 3);
        table.add_column(TableColumnSpec{
            .name = "EXTRA",
            .data_type = DataType::tp_int,
            .kind = ColumnKind::scalar,
            .shape = {},
            .comment = "extra column",
        });
        table.write_scalar_cell("EXTRA", 0, CellValue{std::int32_t{1}});
        table.write_scalar_cell("EXTRA", 1, CellValue{std::int32_t{2}});
        table.flush();
    }
    {
        auto table = Table::open_rw(path);
        table.remove_column("EXTRA");
        check(table.ncolumn() == 1, "ISM ncolumn after remove_column == 1");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.ncolumn() == 1, "ISM reopened ncolumn == 1");
    check(t2.find_column_desc("EXTRA") == nullptr, "ISM removed column not found");
    check(std::get<std::int32_t>(t2.read_scalar_cell("ID", 2)) == 20, "ISM remaining data preserved");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_tsm_add_rows_basic() {
    std::cout << "  TSM add_rows basic... ";
    auto path = make_temp_dir("tsm_add_rows");
    auto table = make_tsm_test_table(path, 2);
    table.add_rows(1);
    check(table.nrow() == 3, "TSM nrow after add_rows(1) == 3");
    table.write_array_int_cell("PIX", 2, {51, 52});
    table.flush();

    auto t2 = Table::open(path);
    check(t2.nrow() == 3, "TSM reopened nrow == 3");
    auto r0 = t2.read_array_int_cell("PIX", 0);
    auto r2 = t2.read_array_int_cell("PIX", 2);
    check(r0.size() == 2 && r0[0] == 1 && r0[1] == 2, "TSM row 0 preserved");
    check(r2.size() == 2 && r2[0] == 51 && r2[1] == 52, "TSM row 2 value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_tsm_remove_rows_basic() {
    std::cout << "  TSM remove_rows basic... ";
    auto path = make_temp_dir("tsm_rm_rows");
    {
        auto table = make_tsm_test_table(path, 3);
        table.flush();
    }
    {
        auto table = Table::open_rw(path);
        table.remove_rows({1});
        check(table.nrow() == 2, "TSM nrow after remove row 1 == 2");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.nrow() == 2, "TSM reopened nrow == 2");
    auto r0 = t2.read_array_int_cell("PIX", 0);
    auto r1 = t2.read_array_int_cell("PIX", 1);
    check(r0.size() == 2 && r0[0] == 1 && r0[1] == 2, "TSM compact row 0");
    check(r1.size() == 2 && r1[0] == 21 && r1[1] == 22, "TSM compact row 1");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_tsm_add_column_basic() {
    std::cout << "  TSM add_column basic... ";
    auto path = make_temp_dir("tsm_add_col");
    auto table = make_tsm_test_table(path, 2);
    table.add_column(TableColumnSpec{
        .name = "EXTRA",
        .data_type = DataType::tp_int,
        .kind = ColumnKind::array,
        .shape = {2},
        .comment = "extra pixels",
    });
    check(table.ncolumn() == 2, "TSM ncolumn after add_column == 2");
    table.write_array_int_cell("EXTRA", 0, {7, 8});
    table.write_array_int_cell("EXTRA", 1, {9, 10});
    table.flush();

    auto t2 = Table::open(path);
    check(t2.ncolumn() == 2, "TSM reopened ncolumn == 2");
    auto base = t2.read_array_int_cell("PIX", 1);
    auto extra = t2.read_array_int_cell("EXTRA", 0);
    check(base.size() == 2 && base[0] == 11 && base[1] == 12, "TSM original column preserved");
    check(extra.size() == 2 && extra[0] == 7 && extra[1] == 8, "TSM new column value");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_tsm_remove_column_basic() {
    std::cout << "  TSM remove_column basic... ";
    auto path = make_temp_dir("tsm_rm_col");
    {
        auto table = make_tsm_test_table(path, 2);
        table.add_column(TableColumnSpec{
            .name = "EXTRA",
            .data_type = DataType::tp_int,
            .kind = ColumnKind::array,
            .shape = {2},
            .comment = "extra pixels",
        });
        table.write_array_int_cell("EXTRA", 0, {3, 4});
        table.flush();
    }
    {
        auto table = Table::open_rw(path);
        table.remove_column("EXTRA");
        check(table.ncolumn() == 1, "TSM ncolumn after remove_column == 1");
        table.flush();
    }

    auto t2 = Table::open(path);
    check(t2.ncolumn() == 1, "TSM reopened ncolumn == 1");
    check(t2.find_column_desc("EXTRA") == nullptr, "TSM removed column not found");
    auto base = t2.read_array_int_cell("PIX", 1);
    check(base.size() == 2 && base[0] == 11 && base[1] == 12, "TSM remaining column preserved");

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "table_api_extensions_test\n";

        // add_rows
        test_add_rows_basic();       // checks: 7
        test_add_rows_zero();        // checks: 1
        test_add_rows_not_writable(); // checks: 1

        // remove_rows
        test_remove_rows_basic();     // checks: 4
        test_remove_rows_empty();     // checks: 1
        test_remove_rows_out_of_range(); // checks: 1
        test_remove_rows_all();       // checks: 2

        // add_column
        test_add_column_basic();      // checks: 5
        test_add_column_duplicate();  // checks: 1

        // remove_column
        test_remove_column_basic();   // checks: 5
        test_remove_column_not_found(); // checks: 1

        // rename_column
        test_rename_column_basic();   // checks: 6
        test_rename_column_not_found(); // checks: 1
        test_rename_column_duplicate_target(); // checks: 1

        // keywords
        test_set_keyword();           // checks: 7
        test_remove_keyword();        // checks: 3
        test_remove_keyword_not_found(); // checks: 1
        test_set_keyword_not_writable(); // checks: 1

        // copy_column
        test_copy_column_scalar();    // checks: 5
        test_copy_column_not_found(); // checks: 1
        test_copy_column_dst_exists(); // checks: 1

        // drop
        test_drop();                  // checks: 2
        test_drop_nonexistent();      // checks: 1

        // combined
        test_add_rows_then_remove();  // checks: 6
        test_add_column_rename_remove(); // checks: 6
        test_keyword_persist_round_trip(); // checks: 2
        test_copy_column_double();    // checks: 3
        test_copy_column_string();    // checks: 3

        // ISM mutation parity
        test_ism_open_rw_write();     // checks: 1
        test_ism_add_rows_basic();    // checks: 6
        test_ism_remove_rows_basic(); // checks: 4
        test_ism_add_column_basic();  // checks: 6
        test_ism_remove_column_basic(); // checks: 4

        // TSM mutation parity
        test_tsm_add_rows_basic();    // checks: 4
        test_tsm_remove_rows_basic(); // checks: 4
        test_tsm_add_column_basic();  // checks: 4
        test_tsm_remove_column_basic(); // checks: 4

        std::cout << "all table_api_extensions tests passed (" << check_count << " checks)\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
