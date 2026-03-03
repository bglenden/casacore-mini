// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// bench_casacore_write.cpp -- casacore-original: write benchmark timings.
//
// Build: clang++ -std=c++20 -O2 $(pkg-config --cflags --libs casacore) \
//        -o bench_casacore_write bench_casacore_write.cpp
//
// Usage: bench_casacore_write <tmp_dir>
//   Creates fresh SSM tables in <tmp_dir>/pass_N/, reports write timings.

#include <casacore/casa/Arrays/Array.h>
#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/tables/DataMan/StandardStMan.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

using namespace casacore;
using Clock = std::chrono::steady_clock;

static constexpr uint64_t kNumRows = 40000;
static constexpr int kArrDim0 = 4;
static constexpr int kArrDim1 = 64;
static constexpr int kArrSize = kArrDim0 * kArrDim1;
static constexpr int kPasses = 3;
static constexpr int kFullRepeats = 3;
static constexpr int kScalarRepeats = 60;
static constexpr int kArrayRepeats = 3;

static double elapsed_sec(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

static double median3(double a, double b, double c) {
    double arr[3] = {a, b, c};
    std::sort(arr, arr + 3);
    return arr[1];
}

// Data generators (same as create_bench_tables.cpp)
static Bool gen_bool(uint64_t row) {
    return (row % 2) == 0;
}
static Int gen_int(uint64_t row) {
    return static_cast<Int>(row * 3 - 100000);
}
static uInt gen_uint(uint64_t row) {
    return static_cast<uInt>(row * 7);
}
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
    return Complex(static_cast<Float>(row) * 0.01F, static_cast<Float>(row) * -0.02F);
}
static DComplex gen_dcomplex(uint64_t row) {
    return DComplex(static_cast<Double>(row) * 0.001, static_cast<Double>(row) * -0.003);
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
    kw.define("CREATOR", String("bench_casacore_write"));
    kw.define("FLOAT_PARAM", Float(2.718281828F));
    kw.define("COMPLEX_PARAM", Complex(1.0F, -1.0F));
    kw.define("DCOMPLEX_PARAM", DComplex(3.14, 2.72));
    kw.define("INT64_PARAM", Int64(9876543210LL));
    kw.define("UINT_PARAM", uInt(42));
    Record sub;
    sub.define("SUB_STR", String("nested_value"));
    sub.define("SUB_INT", Int(999));
    sub.define("SUB_DOUBLE", Double(1.23456789));
    kw.defineRecord("SUB_RECORD", sub);
    Record sub2;
    sub2.define("NAME", String("second_sub"));
    sub2.define("VALUE", Double(42.0));
    kw.defineRecord("SUB_RECORD2", sub2);
}

static TableDesc full_table_desc() {
    TableDesc td("BenchFull", TableDesc::Scratch);
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

static TableDesc scalar_table_desc() {
    TableDesc td("BenchScalar", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Bool>("sc_bool"));
    td.addColumn(ScalarColumnDesc<Int>("sc_int"));
    td.addColumn(ScalarColumnDesc<uInt>("sc_uint"));
    td.addColumn(ScalarColumnDesc<Int64>("sc_int64"));
    td.addColumn(ScalarColumnDesc<Float>("sc_float"));
    td.addColumn(ScalarColumnDesc<Double>("sc_double"));
    td.addColumn(ScalarColumnDesc<Complex>("sc_complex"));
    td.addColumn(ScalarColumnDesc<DComplex>("sc_dcomplex"));
    td.addColumn(ScalarColumnDesc<String>("sc_string"));
    return td;
}

static TableDesc array_table_desc() {
    TableDesc td("BenchArray", TableDesc::Scratch);
    td.addColumn(ArrayColumnDesc<Float>("arr_float", IPosition(2, kArrDim0, kArrDim1),
                                        ColumnDesc::FixedShape));
    td.addColumn(ArrayColumnDesc<Double>("arr_double", IPosition(2, kArrDim0, kArrDim1),
                                         ColumnDesc::FixedShape));
    return td;
}

static void write_full_table(const std::filesystem::path& path, const TableDesc& td) {
    StandardStMan ssm("SSM", 32768);
    SetupNewTable setup(path.string(), td, Table::New);
    setup.bindAll(ssm);
    Table table(setup, kNumRows);

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
    add_keywords(table);
    table.flush();
}

// Full table write: create + all columns + keywords + flush x kFullRepeats
static double bench_full_write(const std::filesystem::path& dir) {
    auto td = full_table_desc();
    auto t0 = Clock::now();
    for (int rep = 0; rep < kFullRepeats; ++rep) {
        auto path = dir / ("full_write_" + std::to_string(rep));
        write_full_table(path, td);
        std::filesystem::remove_all(path);
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// Scalar-only write x kScalarRepeats
static double bench_scalar_write(const std::filesystem::path& dir) {
    auto td = scalar_table_desc();
    auto t0 = Clock::now();
    for (int rep = 0; rep < kScalarRepeats; ++rep) {
        auto path = dir / ("scalar_write_" + std::to_string(rep));
        {
            StandardStMan ssm("SSM", 32768);
            SetupNewTable setup(path.string(), td, Table::New);
            setup.bindAll(ssm);
            Table table(setup, kNumRows);

            ScalarColumn<Bool> col_bool(table, "sc_bool");
            ScalarColumn<Int> col_int(table, "sc_int");
            ScalarColumn<uInt> col_uint(table, "sc_uint");
            ScalarColumn<Int64> col_int64(table, "sc_int64");
            ScalarColumn<Float> col_float(table, "sc_float");
            ScalarColumn<Double> col_double(table, "sc_double");
            ScalarColumn<Complex> col_complex(table, "sc_complex");
            ScalarColumn<DComplex> col_dcomplex(table, "sc_dcomplex");
            ScalarColumn<String> col_string(table, "sc_string");

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
            }
            table.flush();
        } // table destructs before remove_all
        std::filesystem::remove_all(path);
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// Array-only write x kArrayRepeats
static double bench_array_write(const std::filesystem::path& dir) {
    auto td = array_table_desc();
    auto t0 = Clock::now();
    for (int rep = 0; rep < kArrayRepeats; ++rep) {
        auto path = dir / ("array_write_" + std::to_string(rep));
        {
            StandardStMan ssm("SSM", 32768);
            SetupNewTable setup(path.string(), td, Table::New);
            setup.bindAll(ssm);
            Table table(setup, kNumRows);

            ArrayColumn<Float> col_arr_float(table, "arr_float");
            ArrayColumn<Double> col_arr_double(table, "arr_double");

            for (uint64_t r = 0; r < kNumRows; ++r) {
                col_arr_float.put(r, gen_arr_float(r));
                col_arr_double.put(r, gen_arr_double(r));
            }
            table.flush();
        } // table destructs before remove_all
        std::filesystem::remove_all(path);
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tmp_dir>\n";
        return 1;
    }
    std::filesystem::path dir = std::filesystem::absolute(argv[1]);
    std::filesystem::create_directories(dir);

    double full_times[kPasses], scalar_times[kPasses], array_times[kPasses];

    for (int p = 0; p < kPasses; ++p) {
        full_times[p] = bench_full_write(dir);
        scalar_times[p] = bench_scalar_write(dir);
        array_times[p] = bench_array_write(dir);
    }

    double full_med = median3(full_times[0], full_times[1], full_times[2]);
    double scalar_med = median3(scalar_times[0], scalar_times[1], scalar_times[2]);
    double array_med = median3(array_times[0], array_times[1], array_times[2]);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "TIMING full_table_write " << full_med << "\n";
    std::cout << "TIMING scalar_only_write " << scalar_med << "\n";
    std::cout << "TIMING array_only_write " << array_med << "\n";

    return 0;
}
