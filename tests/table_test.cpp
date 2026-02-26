#include "casacore_mini/table.hpp"
#include "casacore_mini/table_column.hpp"
#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("table_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

// ---------------------------------------------------------------------------
// Test: open the SSM fixture table and read structure + cells.
// ---------------------------------------------------------------------------
static void test_open_fixture() {
    std::cout << "  open fixture table... ";

    const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
    const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

    auto table = Table::open(fixture);
    assert(table.nrow() == 5);
    assert(table.ncolumn() == 4);
    assert(table.table_name() == "ssm_table");

    // Column names should include id, value, label, dval.
    const auto& cols = table.columns();
    assert(cols.size() == 4);

    // Verify ScalarColumn reads.
    ScalarColumn<std::int32_t> id_col(table, "id");
    assert(id_col.get(0) == 0);
    assert(id_col.get(4) == 40);
    assert(id_col.nrow() == 5);

    ScalarColumn<float> val_col(table, "value");
    assert(std::fabs(val_col.get(1) - 1.5F) < 1e-6F);

    ScalarColumn<std::string> label_col(table, "label");
    assert(label_col.get(0) == "row_0");
    assert(label_col.get(3) == "row_3");

    ScalarColumn<double> dval_col(table, "dval");
    assert(std::fabs(dval_col.get(2) - 6.28) < 1e-10);

    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: create a table, write with ScalarColumn, flush, reopen, verify.
// ---------------------------------------------------------------------------
static void test_create_and_write_scalars() {
    std::cout << "  create + write scalars... ";

    auto path = make_temp_dir("scalars");
    cleanup(path);

    {
        std::vector<TableColumnSpec> columns = {
            {.name = "ID",    .data_type = DataType::tp_int,    .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "VALUE", .data_type = DataType::tp_float,  .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "DVAL",  .data_type = DataType::tp_double, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "LABEL", .data_type = DataType::tp_string, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
        };

        auto table = Table::create(path, columns, 5);
        assert(table.nrow() == 5);
        assert(table.ncolumn() == 4);

        ScalarColumn<std::int32_t> id_col(table, "ID");
        ScalarColumn<float> val_col(table, "VALUE");
        ScalarColumn<double> dval_col(table, "DVAL");
        ScalarColumn<std::string> label_col(table, "LABEL");

        for (std::uint64_t r = 0; r < 5; ++r) {
            id_col.put(r, static_cast<std::int32_t>(r * 100));
            val_col.put(r, static_cast<float>(r) * 2.5F);
            dval_col.put(r, static_cast<double>(r) * 1.23);
            label_col.put(r, "item_" + std::to_string(r));
        }
        table.flush();
    }

    // Reopen and verify.
    {
        auto table = Table::open(path);
        assert(table.nrow() == 5);
        assert(table.ncolumn() == 4);

        ScalarColumn<std::int32_t> id_col(table, "ID");
        ScalarColumn<float> val_col(table, "VALUE");
        ScalarColumn<double> dval_col(table, "DVAL");
        ScalarColumn<std::string> label_col(table, "LABEL");

        for (std::uint64_t r = 0; r < 5; ++r) {
            assert(id_col.get(r) == static_cast<std::int32_t>(r * 100));
            assert(std::fabs(val_col.get(r) - static_cast<float>(r) * 2.5F) < 1e-6F);
            assert(std::fabs(dval_col.get(r) - static_cast<double>(r) * 1.23) < 1e-10);
            assert(label_col.get(r) == "item_" + std::to_string(r));
        }
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: TableRow read/write round-trip.
// ---------------------------------------------------------------------------
static void test_table_row_round_trip() {
    std::cout << "  TableRow round-trip... ";

    auto path = make_temp_dir("trow");
    cleanup(path);

    {
        std::vector<TableColumnSpec> columns = {
            {.name = "X", .data_type = DataType::tp_int,    .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "Y", .data_type = DataType::tp_double, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "Z", .data_type = DataType::tp_string, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
        };

        auto table = Table::create(path, columns, 3);
        TableRow row(table);

        for (std::uint64_t r = 0; r < 3; ++r) {
            Record rec;
            rec.set("X", RecordValue(static_cast<std::int32_t>(r + 10)));
            rec.set("Y", RecordValue(static_cast<double>(r) * 9.81));
            rec.set("Z", RecordValue(std::string("val_" + std::to_string(r))));
            row.put(r, rec);
        }
        table.flush();
    }

    {
        auto table = Table::open(path);
        TableRow row(table);

        for (std::uint64_t r = 0; r < 3; ++r) {
            Record rec = row.get(r);
            assert(rec.size() == 3);

            auto x = std::get<std::int32_t>(rec.find("X")->storage());
            auto y = std::get<double>(rec.find("Y")->storage());
            auto z = std::get<std::string>(rec.find("Z")->storage());

            assert(x == static_cast<std::int32_t>(r + 10));
            assert(std::fabs(y - static_cast<double>(r) * 9.81) < 1e-10);
            assert(z == "val_" + std::to_string(r));
        }
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: TableRow with column subset.
// ---------------------------------------------------------------------------
static void test_table_row_subset() {
    std::cout << "  TableRow column subset... ";

    auto path = make_temp_dir("subset");
    cleanup(path);

    {
        std::vector<TableColumnSpec> columns = {
            {.name = "A", .data_type = DataType::tp_int,    .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "B", .data_type = DataType::tp_float,  .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "C", .data_type = DataType::tp_string, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
        };

        auto table = Table::create(path, columns, 2);

        ScalarColumn<std::int32_t> a(table, "A");
        ScalarColumn<float> b(table, "B");
        ScalarColumn<std::string> c(table, "C");

        a.put(0, 1); a.put(1, 2);
        b.put(0, 3.0F); b.put(1, 4.0F);
        c.put(0, "hello"); c.put(1, "world");
        table.flush();
    }

    {
        auto table = Table::open(path);

        // Read only columns A and C.
        TableRow row(table, {"A", "C"});
        Record rec = row.get(0);
        assert(rec.size() == 2);
        assert(rec.find("A") != nullptr);
        assert(rec.find("C") != nullptr);
        assert(rec.find("B") == nullptr);

        assert(std::get<std::int32_t>(rec.find("A")->storage()) == 1);
        assert(std::get<std::string>(rec.find("C")->storage()) == "hello");
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: column not found throws.
// ---------------------------------------------------------------------------
static void test_column_not_found() {
    std::cout << "  column not found throws... ";

    // Use the SSM fixture (known to be valid) to test column-not-found.
    const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
    const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

    auto table = Table::open(fixture);

    bool threw = false;
    try {
        ScalarColumn<std::int32_t> bad(table, "NONEXISTENT");
    } catch (const std::runtime_error& e) {
        threw = true;
        std::string msg = e.what();
        assert(msg.find("NONEXISTENT") != std::string::npos);
    }
    assert(threw);

    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: empty table (0 rows).
// ---------------------------------------------------------------------------
static void test_empty_table() {
    std::cout << "  empty table (0 rows)... ";

    auto path = make_temp_dir("empty");
    cleanup(path);

    std::vector<TableColumnSpec> columns = {
        {.name = "V", .data_type = DataType::tp_double, .kind = ColumnKind::scalar,
         .shape = {}, .comment = {}},
    };
    auto table = Table::create(path, columns, 0);
    assert(table.nrow() == 0);
    assert(table.ncolumn() == 1);
    table.flush();

    auto table2 = Table::open(path);
    assert(table2.nrow() == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: table keywords access.
// ---------------------------------------------------------------------------
static void test_keywords() {
    std::cout << "  table keywords... ";

    const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
    const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

    auto table = Table::open(fixture);
    const auto& kw = table.keywords();

    // The fixture may or may not have keywords; at minimum we verify
    // the keywords() method doesn't crash and returns a valid Record.
    (void)kw.size();

    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: table structure via public API (replaces old table_dat escape hatch).
// ---------------------------------------------------------------------------
static void test_table_structure() {
    std::cout << "  table structure via public API... ";

    const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
    const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

    auto table = Table::open(fixture);
    assert(table.nrow() == 5);
    assert(table.ncolumn() == 4);
    assert(!table.columns().empty());

    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: nrow consistency between Table and ScalarColumn.
// ---------------------------------------------------------------------------
static void test_nrow_consistency() {
    std::cout << "  nrow consistency... ";

    // Use the fixture to avoid SSM bucket size edge cases with minimal tables.
    const fs::path source_dir = CASACORE_MINI_SOURCE_DIR;
    const fs::path fixture = source_dir / "tests" / "fixtures" / "ssm_table";

    auto table = Table::open(fixture);
    ScalarColumn<std::int32_t> col(table, "id");

    assert(table.nrow() == 5);
    assert(col.nrow() == 5);

    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: mixed writes (ScalarColumn then TableRow on same table).
// ---------------------------------------------------------------------------
static void test_mixed_writes() {
    std::cout << "  mixed ScalarColumn + TableRow writes... ";

    auto path = make_temp_dir("mixed");
    cleanup(path);

    {
        std::vector<TableColumnSpec> columns = {
            {.name = "K", .data_type = DataType::tp_int,    .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
            {.name = "V", .data_type = DataType::tp_string, .kind = ColumnKind::scalar,
             .shape = {}, .comment = {}},
        };

        auto table = Table::create(path, columns, 4);

        // Write rows 0-1 via ScalarColumn.
        ScalarColumn<std::int32_t> k(table, "K");
        ScalarColumn<std::string> v(table, "V");
        k.put(0, 100); v.put(0, "alpha");
        k.put(1, 200); v.put(1, "beta");

        // Write rows 2-3 via TableRow.
        TableRow row(table);
        {
            Record rec;
            rec.set("K", RecordValue(std::int32_t{300}));
            rec.set("V", RecordValue(std::string("gamma")));
            row.put(2, rec);
        }
        {
            Record rec;
            rec.set("K", RecordValue(std::int32_t{400}));
            rec.set("V", RecordValue(std::string("delta")));
            row.put(3, rec);
        }
        table.flush();
    }

    // Verify all 4 rows.
    {
        auto table = Table::open(path);
        ScalarColumn<std::int32_t> k(table, "K");
        ScalarColumn<std::string> v(table, "V");

        assert(k.get(0) == 100 && v.get(0) == "alpha");
        assert(k.get(1) == 200 && v.get(1) == "beta");
        assert(k.get(2) == 300 && v.get(2) == "gamma");
        assert(k.get(3) == 400 && v.get(3) == "delta");
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: path() and table_name() accessors.
// ---------------------------------------------------------------------------
static void test_path_and_name() {
    std::cout << "  path() and table_name()... ";

    auto path = make_temp_dir("pathtest");
    cleanup(path);

    std::vector<TableColumnSpec> columns = {
        {.name = "X", .data_type = DataType::tp_int, .kind = ColumnKind::scalar,
         .shape = {}, .comment = {}},
    };
    auto table = Table::create(path, columns, 0);

    assert(table.path() == path);
    // table_name() should return the basename of the path.
    assert(table.table_name() == path.filename().string());

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Test: small tables (1 column, few rows) — regression for bucket size bug.
// These mirror the casacore-original validation test.
// ---------------------------------------------------------------------------
static void test_small_table_7rows() {
    std::cout << "  small table (1 col, 7 rows)... ";

    auto path = make_temp_dir("small7");
    cleanup(path);

    std::vector<TableColumnSpec> columns = {
        {.name = "N", .data_type = DataType::tp_int, .kind = ColumnKind::scalar,
         .shape = {}, .comment = {}},
    };
    {
        auto table = Table::create(path, columns, 7);
        ScalarColumn<std::int32_t> col(table, "N");
        for (std::uint64_t r = 0; r < 7; ++r) {
            col.put(r, static_cast<std::int32_t>(r));
        }
        table.flush();
    }

    {
        auto table = Table::open(path);
        assert(table.nrow() == 7);
        ScalarColumn<std::int32_t> col(table, "N");
        for (std::uint64_t r = 0; r < 7; ++r) {
            assert(col.get(r) == static_cast<std::int32_t>(r));
        }
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_small_table_1row() {
    std::cout << "  small table (1 col, 1 row)... ";

    auto path = make_temp_dir("small1");
    cleanup(path);

    std::vector<TableColumnSpec> columns = {
        {.name = "N", .data_type = DataType::tp_int, .kind = ColumnKind::scalar,
         .shape = {}, .comment = {}},
    };
    {
        auto table = Table::create(path, columns, 1);
        ScalarColumn<std::int32_t> col(table, "N");
        col.put(0, 42);
        table.flush();
    }

    {
        auto table = Table::open(path);
        assert(table.nrow() == 1);
        ScalarColumn<std::int32_t> col(table, "N");
        assert(col.get(0) == 42);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Helper: build a TableDatFull with ISM columns and write it to disk.
// ---------------------------------------------------------------------------
static TableDatFull make_ism_table_dat(std::uint64_t row_count) {
    TableDatFull full;
    full.table_version = 2;
    full.row_count = row_count;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "ism_test";

    auto make_col = [](const std::string& name, DataType dtype) {
        ColumnDesc col;
        col.kind = ColumnKind::scalar;
        col.name = name;
        col.data_type = dtype;
        col.dm_type = "IncrementalStMan";
        col.dm_group = "ISMData";
        col.version = 1;
        col.type_string = (dtype == DataType::tp_double)
                              ? "ScalarColumnDesc<Double  >"
                              : (dtype == DataType::tp_int ? "ScalarColumnDesc<Int     >"
                                                          : "ScalarColumnDesc<Bool>");
        return col;
    };

    full.table_desc.columns.push_back(make_col("time", DataType::tp_double));
    full.table_desc.columns.push_back(make_col("antenna", DataType::tp_int));

    StorageManagerSetup sm;
    sm.type_name = "IncrementalStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : full.table_desc.columns) {
        ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        full.column_setups.push_back(cms);
    }

    full.post_td_row_count = row_count;
    return full;
}

// ---------------------------------------------------------------------------
// Test: ISM read through Table API (ScalarColumn).
// ---------------------------------------------------------------------------
static void test_ism_via_table_api() {
    std::cout << "  ISM via Table API (ScalarColumn)... ";

    constexpr std::uint64_t kRows = 10;
    auto path = make_temp_dir("ism_table");
    cleanup(path);

    auto full = make_ism_table_dat(kRows);

    // Write table directory (table.dat + metadata).
    write_table_directory(path.string(), full);

    // Write ISM data file (table.f0).
    IsmWriter writer;
    writer.setup(full.table_desc.columns, kRows, false, "ISMData");
    for (std::uint64_t r = 0; r < kRows; ++r) {
        writer.write_cell(0, CellValue{1000.0 + static_cast<double>(r)}, r);
        writer.write_cell(1, CellValue{static_cast<std::int32_t>(r * 10)}, r);
    }
    writer.write_file(path.string(), 0);

    // Open via Table API and read with ScalarColumn.
    auto table = Table::open(path);
    assert(table.nrow() == kRows);
    assert(table.ncolumn() == 2);

    ScalarColumn<double> time_col(table, "time");
    ScalarColumn<std::int32_t> ant_col(table, "antenna");

    for (std::uint64_t r = 0; r < kRows; ++r) {
        assert(std::fabs(time_col.get(r) - (1000.0 + static_cast<double>(r))) < 1e-10);
        assert(ant_col.get(r) == static_cast<std::int32_t>(r * 10));
    }

    cleanup(path);
    std::cout << "PASS\n";
}

// ---------------------------------------------------------------------------
// Helper: build a TableDatFull with a TSM float array column.
// ---------------------------------------------------------------------------
static TableDatFull make_tsm_table_dat(std::uint64_t row_count,
                                        const std::vector<std::int32_t>& shape) {
    TableDatFull full;
    full.table_version = 2;
    full.row_count = row_count;
    full.big_endian = true;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "tsm_test";

    ColumnDesc col;
    col.kind = ColumnKind::array;
    col.name = "data";
    col.data_type = DataType::tp_float;
    col.dm_type = "TiledColumnStMan";
    col.dm_group = "TiledCol";
    col.type_string = "ArrayColumnDesc<Float   >";
    col.version = 1;
    col.ndim = static_cast<std::int32_t>(shape.size());
    col.shape.assign(shape.begin(), shape.end());
    col.options = 4;  // ColumnDesc::FixedShape
    full.table_desc.columns.push_back(col);

    StorageManagerSetup sm;
    sm.type_name = "TiledColumnStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    ColumnManagerSetup cms;
    cms.column_name = "data";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape.assign(shape.begin(), shape.end());
    full.column_setups.push_back(cms);

    full.post_td_row_count = row_count;
    return full;
}

// ---------------------------------------------------------------------------
// Test: TSM read through Table API (ArrayColumn).
// ---------------------------------------------------------------------------
static void test_tsm_via_table_api() {
    std::cout << "  TSM via Table API (ArrayColumn)... ";

    constexpr std::uint64_t kRows = 5;
    const std::vector<std::int32_t> shape = {4, 8};  // 32 floats per cell
    constexpr std::uint64_t kCellElements = 32;

    auto path = make_temp_dir("tsm_table");
    cleanup(path);

    auto full = make_tsm_table_dat(kRows, shape);

    // Write table directory.
    write_table_directory(path.string(), full);

    // Write TSM data files.
    TiledStManWriter tsm_writer;
    tsm_writer.setup("TiledColumnStMan", "TiledCol", full.table_desc.columns, kRows);
    for (std::uint64_t r = 0; r < kRows; ++r) {
        std::vector<float> cell_data(kCellElements);
        for (std::uint64_t i = 0; i < kCellElements; ++i) {
            cell_data[i] = static_cast<float>(r * 100 + i);
        }
        tsm_writer.write_float_cell(0, cell_data, r);
    }
    tsm_writer.write_files(path.string(), 0);

    // Open via Table API and read with ArrayColumn.
    auto table = Table::open(path);
    assert(table.nrow() == kRows);
    assert(table.ncolumn() == 1);

    ArrayColumn<float> data_col(table, "data");

    for (std::uint64_t r = 0; r < kRows; ++r) {
        auto values = data_col.get(r);
        assert(values.size() == kCellElements);
        for (std::uint64_t i = 0; i < kCellElements; ++i) {
            const float expected = static_cast<float>(r * 100 + i);
            assert(std::fabs(values[i] - expected) < 1e-6F);
        }
    }

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
    std::cout << "table_test\n";

    test_open_fixture();
    test_create_and_write_scalars();
    test_table_row_round_trip();
    test_table_row_subset();
    test_column_not_found();
    test_empty_table();
    test_keywords();
    test_table_structure();
    test_nrow_consistency();
    test_mixed_writes();
    test_path_and_name();
    test_small_table_7rows();
    test_small_table_1row();
    test_ism_via_table_api();
    test_tsm_via_table_api();

    std::cout << "all table tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
