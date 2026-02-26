// create_bench_tables.cpp -- casacore-original: create 3 benchmark tables
// (SSM, ISM, TSM) with 500k rows for read-benchmark comparison.
//
// Build: clang++ -std=c++20 -O2 $(pkg-config --cflags --libs casacore) \
//        -o create_bench_tables create_bench_tables.cpp
//
// Usage: create_bench_tables <output_dir>
//   Creates: <output_dir>/bench_ssm/
//            <output_dir>/bench_ism/
//            <output_dir>/bench_tsm/

#include <casacore/casa/Arrays/Array.h>
#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/tables/DataMan/StandardStMan.h>
#include <casacore/tables/DataMan/TiledShapeStMan.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>

#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace casacore;

static constexpr uint64_t kNumRows = 500000;
static constexpr int kArrDim0 = 4;
static constexpr int kArrDim1 = 64;
static constexpr int kArrSize = kArrDim0 * kArrDim1; // 256

// Deterministic data generators (row-index-based for verifiability)
static Bool gen_bool(uint64_t row) { return (row % 2) == 0; }
static Int gen_int(uint64_t row) { return static_cast<Int>(row * 3 - 100000); }
static uInt gen_uint(uint64_t row) { return static_cast<uInt>(row * 7); }
static Int64 gen_int64(uint64_t row) {
    return static_cast<Int64>(row) * 100003LL - 25000000000LL;
}
static Float gen_float(uint64_t row) {
    return static_cast<Float>(row) * 0.001F + 1.5F;
}
static Double gen_double(uint64_t row) {
    return static_cast<Double>(row) * 0.000001 + 3.14159265358979;
}
static Complex gen_complex(uint64_t row) {
    return Complex(static_cast<Float>(row) * 0.01F,
                   static_cast<Float>(row) * -0.02F);
}
static DComplex gen_dcomplex(uint64_t row) {
    return DComplex(static_cast<Double>(row) * 0.001,
                    static_cast<Double>(row) * -0.003);
}
static String gen_string(uint64_t row) {
    return "row_" + std::to_string(row) + "_value";
}

static Array<Float> gen_arr_float(uint64_t row) {
    IPosition shape(2, kArrDim0, kArrDim1);
    Array<Float> arr(shape);
    Float* data = arr.data();
    for (int i = 0; i < kArrSize; ++i) {
        data[i] = static_cast<Float>(row * 256 + i) * 0.01F;
    }
    return arr;
}

static Array<Double> gen_arr_double(uint64_t row) {
    IPosition shape(2, kArrDim0, kArrDim1);
    Array<Double> arr(shape);
    Double* data = arr.data();
    for (int i = 0; i < kArrSize; ++i) {
        data[i] = static_cast<Double>(row * 256 + i) * 0.0001;
    }
    return arr;
}

// Add ~20 keywords to a table
static void add_keywords(Table& table) {
    TableRecord& kw = table.rwKeywordSet();
    kw.define("VERSION", String("1.0.0"));
    kw.define("TELESCOPE", String("VLA"));
    kw.define("OBSERVER", String("bench_test"));
    kw.define("PROJECT_ID", String("BENCH-001"));
    kw.define("NUM_ROWS", Int(static_cast<Int>(kNumRows)));
    kw.define("TOTAL_TIME", Double(3600.0));
    kw.define("REF_FREQ", Double(1.4e9));
    kw.define("CHAN_WIDTH", Double(1e6));
    kw.define("NUM_CHAN", Int(64));
    kw.define("NUM_POL", Int(4));
    kw.define("IS_CALIBRATED", Bool(false));
    kw.define("CREATOR", String("create_bench_tables"));
    kw.define("FLOAT_PARAM", Float(2.718281828F));
    kw.define("COMPLEX_PARAM", Complex(1.0F, -1.0F));
    kw.define("DCOMPLEX_PARAM", DComplex(3.14, 2.72));
    kw.define("INT64_PARAM", Int64(9876543210LL));
    kw.define("UINT_PARAM", uInt(42));

    // Nested sub-record
    Record sub;
    sub.define("SUB_STR", String("nested_value"));
    sub.define("SUB_INT", Int(999));
    sub.define("SUB_DOUBLE", Double(1.23456789));
    kw.defineRecord("SUB_RECORD", sub);

    // Second nested sub-record
    Record sub2;
    sub2.define("NAME", String("second_sub"));
    sub2.define("VALUE", Double(42.0));
    kw.defineRecord("SUB_RECORD2", sub2);
}

// Build a TableDesc with 9 scalar + 2 array columns
static TableDesc make_table_desc() {
    TableDesc td("BenchTable", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Bool>("sc_bool"));
    td.addColumn(ScalarColumnDesc<Int>("sc_int"));
    td.addColumn(ScalarColumnDesc<uInt>("sc_uint"));
    td.addColumn(ScalarColumnDesc<Int64>("sc_int64"));
    td.addColumn(ScalarColumnDesc<Float>("sc_float"));
    td.addColumn(ScalarColumnDesc<Double>("sc_double"));
    td.addColumn(ScalarColumnDesc<Complex>("sc_complex"));
    td.addColumn(ScalarColumnDesc<DComplex>("sc_dcomplex"));
    td.addColumn(ScalarColumnDesc<String>("sc_string"));
    td.addColumn(ArrayColumnDesc<Float>("arr_float", IPosition(2, kArrDim0, kArrDim1),
                                         ColumnDesc::FixedShape));
    td.addColumn(ArrayColumnDesc<Double>("arr_double", IPosition(2, kArrDim0, kArrDim1),
                                          ColumnDesc::FixedShape));
    return td;
}

// Populate all rows
static void populate(Table& table) {
    ScalarColumn<Bool> col_bool(table, "sc_bool");
    ScalarColumn<Int> col_int(table, "sc_int");
    ScalarColumn<uInt> col_uint(table, "sc_uint");
    ScalarColumn<Int64> col_int64(table, "sc_int64");
    ScalarColumn<Float> col_float(table, "sc_float");
    ScalarColumn<Double> col_double(table, "sc_double");
    ScalarColumn<Complex> col_complex(table, "sc_complex");
    ScalarColumn<DComplex> col_dcomplex(table, "sc_dcomplex");
    ScalarColumn<String> col_string(table, "sc_string");
    ArrayColumn<Float> col_arr_float(table, "arr_float");
    ArrayColumn<Double> col_arr_double(table, "arr_double");

    for (uint64_t r = 0; r < kNumRows; ++r) {
        col_bool.put(r, gen_bool(r));
        col_int.put(r, gen_int(r));
        col_uint.put(r, gen_uint(r));
        col_int64.put(r, gen_int64(r));
        col_float.put(r, gen_float(r));
        col_double.put(r, gen_double(r));
        col_complex.put(r, gen_complex(r));
        col_dcomplex.put(r, gen_dcomplex(r));
        col_string.put(r, gen_string(r));
        col_arr_float.put(r, gen_arr_float(r));
        col_arr_double.put(r, gen_arr_double(r));
    }
}

// Create SSM table (all columns in StandardStMan)
static void create_ssm_table(const std::filesystem::path& dir) {
    auto path = dir / "bench_ssm";
    std::cout << "Creating SSM table: " << path << " ..." << std::flush;
    TableDesc td = make_table_desc();
    StandardStMan ssm("SSM", 32768);
    SetupNewTable setup(path.string(), td, Table::New);
    setup.bindAll(ssm);
    Table table(setup, kNumRows);
    populate(table);
    add_keywords(table);
    table.flush();
    std::cout << " done (" << kNumRows << " rows)\n";
}

// Create ISM table (ISM for scalars, SSM for arrays)
static void create_ism_table(const std::filesystem::path& dir) {
    auto path = dir / "bench_ism";
    std::cout << "Creating ISM table: " << path << " ..." << std::flush;
    TableDesc td = make_table_desc();
    IncrementalStMan ism("ISM");
    StandardStMan ssm("SSM_arr", 32768);
    SetupNewTable setup(path.string(), td, Table::New);
    // Bind scalars to ISM
    setup.bindColumn("sc_bool", ism);
    setup.bindColumn("sc_int", ism);
    setup.bindColumn("sc_uint", ism);
    setup.bindColumn("sc_int64", ism);
    setup.bindColumn("sc_float", ism);
    setup.bindColumn("sc_double", ism);
    setup.bindColumn("sc_complex", ism);
    setup.bindColumn("sc_dcomplex", ism);
    setup.bindColumn("sc_string", ism);
    // Bind arrays to SSM
    setup.bindColumn("arr_float", ssm);
    setup.bindColumn("arr_double", ssm);
    Table table(setup, kNumRows);
    populate(table);
    add_keywords(table);
    table.flush();
    std::cout << " done (" << kNumRows << " rows)\n";
}

// Create TSM table (TSM for arrays, SSM for scalars)
static void create_tsm_table(const std::filesystem::path& dir) {
    auto path = dir / "bench_tsm";
    std::cout << "Creating TSM table: " << path << " ..." << std::flush;
    TableDesc td = make_table_desc();
    StandardStMan ssm("SSM_sc", 32768);
    // TiledShapeStMan for array columns.
    // Tile shape: full array shape (4,64) x some rows.
    IPosition tile_shape(3, kArrDim0, kArrDim1, 512);
    TiledShapeStMan tsm_float("TSM_float", tile_shape);
    TiledShapeStMan tsm_double("TSM_double", tile_shape);
    SetupNewTable setup(path.string(), td, Table::New);
    // Bind scalars to SSM
    setup.bindColumn("sc_bool", ssm);
    setup.bindColumn("sc_int", ssm);
    setup.bindColumn("sc_uint", ssm);
    setup.bindColumn("sc_int64", ssm);
    setup.bindColumn("sc_float", ssm);
    setup.bindColumn("sc_double", ssm);
    setup.bindColumn("sc_complex", ssm);
    setup.bindColumn("sc_dcomplex", ssm);
    setup.bindColumn("sc_string", ssm);
    // Bind arrays to TSM
    setup.bindColumn("arr_float", tsm_float);
    setup.bindColumn("arr_double", tsm_double);
    Table table(setup, kNumRows);
    populate(table);
    add_keywords(table);
    table.flush();
    std::cout << " done (" << kNumRows << " rows)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <output_dir>\n";
        return 1;
    }
    std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);

    create_ssm_table(dir);
    create_ism_table(dir);
    create_tsm_table(dir);

    std::cout << "All benchmark tables created in: " << dir << "\n";
    return 0;
}
