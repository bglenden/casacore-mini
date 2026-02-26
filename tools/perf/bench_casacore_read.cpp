// bench_casacore_read.cpp -- casacore-original: read benchmark timings.
//
// Build: clang++ -std=c++20 -O2 $(pkg-config --cflags --libs casacore) \
//        -o bench_casacore_read bench_casacore_read.cpp
//
// Usage: bench_casacore_read <table_dir>
//   Reads table at <table_dir>, runs 4 read operations, reports timings.
//   Run once per table (bench_ssm, bench_ism, bench_tsm).

#include <casacore/casa/Arrays/Array.h>
#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/tables/Tables/TableRecord.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableColumn.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace casacore;
using Clock = std::chrono::steady_clock;

static constexpr int kPasses = 3;
static constexpr int kScalarRepeats = 1;
static constexpr int kKeywordIter = 100000;

static double elapsed_sec(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double>(end - start).count();
}

static double median3(double a, double b, double c) {
    double arr[3] = {a, b, c};
    std::sort(arr, arr + 3);
    return arr[1];
}

// 1. Scalar sequential read: all 9 scalar columns x all rows x kScalarRepeats
static double bench_scalar_seq_read(Table& table) {
    auto nrow = table.nrow();
    ScalarColumn<Bool> col_bool(table, "sc_bool");
    ScalarColumn<Int> col_int(table, "sc_int");
    ScalarColumn<uInt> col_uint(table, "sc_uint");
    ScalarColumn<Int64> col_int64(table, "sc_int64");
    ScalarColumn<Float> col_float(table, "sc_float");
    ScalarColumn<Double> col_double(table, "sc_double");
    ScalarColumn<Complex> col_complex(table, "sc_complex");
    ScalarColumn<DComplex> col_dcomplex(table, "sc_dcomplex");
    ScalarColumn<String> col_string(table, "sc_string");

    volatile Bool vb;
    volatile Int vi;
    volatile uInt vu;
    volatile Int64 vi64;
    volatile Float vf;
    volatile Double vd;
    volatile Float vc_r;
    volatile Double vdc_r;

    auto t0 = Clock::now();
    for (int rep = 0; rep < kScalarRepeats; ++rep) {
        for (uInt64 r = 0; r < nrow; ++r) {
            vb = col_bool(r);
            vi = col_int(r);
            vu = col_uint(r);
            vi64 = col_int64(r);
            vf = col_float(r);
            vd = col_double(r);
            Complex c = col_complex(r);
            vc_r = c.real();
            DComplex dc = col_dcomplex(r);
            vdc_r = dc.real();
            String s = col_string(r);
        }
    }
    auto t1 = Clock::now();
    // Suppress unused warnings
    (void)vb; (void)vi; (void)vu; (void)vi64; (void)vf; (void)vd;
    (void)vc_r; (void)vdc_r;
    return elapsed_sec(t0, t1);
}

// 2. Array sequential read: both array columns x all rows
static double bench_array_seq_read(Table& table) {
    auto nrow = table.nrow();
    ArrayColumn<Float> col_float(table, "arr_float");
    ArrayColumn<Double> col_double(table, "arr_double");

    auto t0 = Clock::now();
    for (uInt64 r = 0; r < nrow; ++r) {
        Array<Float> af = col_float(r);
        Array<Double> ad = col_double(r);
        (void)af;
        (void)ad;
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// 3. Keyword access: read all ~20 keywords x many iterations
static double bench_keyword_access(Table& table) {
    auto t0 = Clock::now();
    for (int i = 0; i < kKeywordIter; ++i) {
        const TableRecord& kw = table.keywordSet();
        String s1 = kw.asString("VERSION");
        String s2 = kw.asString("TELESCOPE");
        String s3 = kw.asString("OBSERVER");
        String s4 = kw.asString("PROJECT_ID");
        Int n1 = kw.asInt("NUM_ROWS");
        Double d1 = kw.asDouble("TOTAL_TIME");
        Double d2 = kw.asDouble("REF_FREQ");
        Double d3 = kw.asDouble("CHAN_WIDTH");
        Int n2 = kw.asInt("NUM_CHAN");
        Int n3 = kw.asInt("NUM_POL");
        Bool b1 = kw.asBool("IS_CALIBRATED");
        String s5 = kw.asString("CREATOR");
        Float f1 = kw.asFloat("FLOAT_PARAM");
        Complex c1 = kw.asComplex("COMPLEX_PARAM");
        DComplex dc1 = kw.asDComplex("DCOMPLEX_PARAM");
        Int64 i64 = kw.asInt64("INT64_PARAM");
        uInt u1 = kw.asuInt("UINT_PARAM");
        const Record& sub = kw.subRecord("SUB_RECORD");
        String ss = sub.asString("SUB_STR");
        Int si = sub.asInt("SUB_INT");
        (void)s1; (void)s2; (void)s3; (void)s4;
        (void)n1; (void)d1; (void)d2; (void)d3;
        (void)n2; (void)n3; (void)b1; (void)s5;
        (void)f1; (void)c1; (void)dc1; (void)i64; (void)u1;
        (void)ss; (void)si;
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// 4. Row-oriented read: all columns per row, all rows
static double bench_row_oriented_read(Table& table) {
    auto nrow = table.nrow();
    ScalarColumn<Bool> col_bool(table, "sc_bool");
    ScalarColumn<Int> col_int(table, "sc_int");
    ScalarColumn<uInt> col_uint(table, "sc_uint");
    ScalarColumn<Int64> col_int64(table, "sc_int64");
    ScalarColumn<Float> col_float(table, "sc_float");
    ScalarColumn<Double> col_double(table, "sc_double");
    ScalarColumn<Complex> col_complex(table, "sc_complex");
    ScalarColumn<DComplex> col_dcomplex(table, "sc_dcomplex");
    ScalarColumn<String> col_string(table, "sc_string");
    ArrayColumn<Float> arr_float(table, "arr_float");
    ArrayColumn<Double> arr_double(table, "arr_double");

    auto t0 = Clock::now();
    for (uInt64 r = 0; r < nrow; ++r) {
        Bool b = col_bool(r);
        Int i = col_int(r);
        uInt u = col_uint(r);
        Int64 i64 = col_int64(r);
        Float f = col_float(r);
        Double d = col_double(r);
        Complex c = col_complex(r);
        DComplex dc = col_dcomplex(r);
        String s = col_string(r);
        Array<Float> af = arr_float(r);
        Array<Double> ad = arr_double(r);
        (void)b; (void)i; (void)u; (void)i64;
        (void)f; (void)d; (void)c; (void)dc; (void)s;
        (void)af; (void)ad;
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <table_dir>\n";
        return 1;
    }
    std::string table_path = argv[1];

    Table table(table_path, Table::Old);
    std::cerr << "Opened table: " << table_path
              << " (" << table.nrow() << " rows)\n";

    // Run each benchmark kPasses times, report median
    double scalar_times[kPasses], array_times[kPasses],
           keyword_times[kPasses], row_times[kPasses];

    for (int p = 0; p < kPasses; ++p) {
        scalar_times[p] = bench_scalar_seq_read(table);
        array_times[p] = bench_array_seq_read(table);
        keyword_times[p] = bench_keyword_access(table);
        row_times[p] = bench_row_oriented_read(table);
    }

    double scalar_med = median3(scalar_times[0], scalar_times[1], scalar_times[2]);
    double array_med = median3(array_times[0], array_times[1], array_times[2]);
    double keyword_med = median3(keyword_times[0], keyword_times[1], keyword_times[2]);
    double row_med = median3(row_times[0], row_times[1], row_times[2]);

    // Machine-readable output
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "TIMING scalar_seq_read " << scalar_med << "\n";
    std::cout << "TIMING array_seq_read " << array_med << "\n";
    std::cout << "TIMING keyword_access " << keyword_med << "\n";
    std::cout << "TIMING row_oriented_read " << row_med << "\n";

    return 0;
}
