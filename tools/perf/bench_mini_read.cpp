// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// bench_mini_read.cpp -- casacore-mini: read benchmark timings.
//
// Built via CMake (bench_mini_read target).
//
// Usage: bench_mini_read <table_dir>
//   Reads table at <table_dir>, runs 4 read operations, reports timings.

#include "casacore_mini/record.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_column.hpp"

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace cm = casacore_mini;
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

// 1. Scalar sequential read: 9 scalar columns x all rows x kScalarRepeats
static double bench_scalar_seq_read(cm::Table& table) {
    auto nrow = table.nrow();
    cm::ScalarColumn<bool> col_bool(table, "sc_bool");
    cm::ScalarColumn<std::int32_t> col_int(table, "sc_int");
    cm::ScalarColumn<std::uint32_t> col_uint(table, "sc_uint");
    cm::ScalarColumn<std::int64_t> col_int64(table, "sc_int64");
    cm::ScalarColumn<float> col_float(table, "sc_float");
    cm::ScalarColumn<double> col_double(table, "sc_double");
    cm::ScalarColumn<std::complex<float>> col_complex(table, "sc_complex");
    cm::ScalarColumn<std::complex<double>> col_dcomplex(table, "sc_dcomplex");
    cm::ScalarColumn<std::string> col_string(table, "sc_string");

    auto t0 = Clock::now();
    for (int rep = 0; rep < kScalarRepeats; ++rep) {
        for (std::uint64_t r = 0; r < nrow; ++r) {
            volatile bool vb = col_bool.get(r);
            volatile std::int32_t vi = col_int.get(r);
            volatile std::uint32_t vu = col_uint.get(r);
            volatile std::int64_t vi64 = col_int64.get(r);
            volatile float vf = col_float.get(r);
            volatile double vd = col_double.get(r);
            auto vc = col_complex.get(r);
            auto vdc = col_dcomplex.get(r);
            std::string s = col_string.get(r);
            (void)vb;
            (void)vi;
            (void)vu;
            (void)vi64;
            (void)vf;
            (void)vd;
            (void)vc;
            (void)vdc;
            (void)s;
        }
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// 2. Array sequential read: both array columns x all rows
static double bench_array_seq_read(cm::Table& table) {
    auto nrow = table.nrow();
    cm::ArrayColumn<float> col_float(table, "arr_float");
    cm::ArrayColumn<double> col_double(table, "arr_double");

    auto t0 = Clock::now();
    for (std::uint64_t r = 0; r < nrow; ++r) {
        auto af = col_float.get(r);
        auto ad = col_double.get(r);
        (void)af;
        (void)ad;
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// 3. Keyword access: look up each keyword by name and extract typed value,
//    matching what the casacore-original benchmark does.
static double bench_keyword_access(cm::Table& table) {
    const auto& kw = table.keywords();
    auto t0 = Clock::now();
    for (int i = 0; i < kKeywordIter; ++i) {
        auto get_str = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::string>(&v->storage()) : nullptr;
        };
        auto get_i32 = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::int32_t>(&v->storage()) : nullptr;
        };
        auto get_u32 = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::uint32_t>(&v->storage()) : nullptr;
        };
        auto get_i64 = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::int64_t>(&v->storage()) : nullptr;
        };
        auto get_f32 = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<float>(&v->storage()) : nullptr;
        };
        auto get_f64 = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<double>(&v->storage()) : nullptr;
        };
        auto get_bool = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<bool>(&v->storage()) : nullptr;
        };
        auto get_cx = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::complex<float>>(&v->storage()) : nullptr;
        };
        auto get_dcx = [&](const char* k) {
            const auto* v = kw.find(k);
            return v ? std::get_if<std::complex<double>>(&v->storage()) : nullptr;
        };
        auto get_rec = [&](const char* k) -> const cm::Record* {
            const auto* v = kw.find(k);
            if (!v)
                return nullptr;
            auto* rp = std::get_if<cm::RecordValue::record_ptr>(&v->storage());
            return rp ? rp->get() : nullptr;
        };

        volatile const void* sink;
        sink = get_str("VERSION");
        sink = get_str("TELESCOPE");
        sink = get_str("OBSERVER");
        sink = get_str("PROJECT_ID");
        sink = get_i32("NUM_ROWS");
        sink = get_f64("TOTAL_TIME");
        sink = get_f64("REF_FREQ");
        sink = get_f64("CHAN_WIDTH");
        sink = get_i32("NUM_CHAN");
        sink = get_i32("NUM_POL");
        sink = get_bool("IS_CALIBRATED");
        sink = get_str("CREATOR");
        sink = get_f32("FLOAT_PARAM");
        sink = get_cx("COMPLEX_PARAM");
        sink = get_dcx("DCOMPLEX_PARAM");
        sink = get_i64("INT64_PARAM");
        sink = get_u32("UINT_PARAM");
        const auto* sub = get_rec("SUB_RECORD");
        if (sub != nullptr) {
            const auto* sv = sub->find("SUB_STR");
            sink = sv != nullptr ? std::get_if<std::string>(&sv->storage()) : nullptr;
            const auto* si = sub->find("SUB_INT");
            sink = si != nullptr ? std::get_if<std::int32_t>(&si->storage()) : nullptr;
        }
        (void)sink;
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// 4. Row-oriented read: all columns per row, all rows
static double bench_row_oriented_read(cm::Table& table) {
    auto nrow = table.nrow();
    cm::ScalarColumn<bool> col_bool(table, "sc_bool");
    cm::ScalarColumn<std::int32_t> col_int(table, "sc_int");
    cm::ScalarColumn<std::uint32_t> col_uint(table, "sc_uint");
    cm::ScalarColumn<std::int64_t> col_int64(table, "sc_int64");
    cm::ScalarColumn<float> col_float(table, "sc_float");
    cm::ScalarColumn<double> col_double(table, "sc_double");
    cm::ScalarColumn<std::complex<float>> col_complex(table, "sc_complex");
    cm::ScalarColumn<std::complex<double>> col_dcomplex(table, "sc_dcomplex");
    cm::ScalarColumn<std::string> col_string(table, "sc_string");
    cm::ArrayColumn<float> arr_float(table, "arr_float");
    cm::ArrayColumn<double> arr_double(table, "arr_double");

    auto t0 = Clock::now();
    for (std::uint64_t r = 0; r < nrow; ++r) {
        volatile bool vb = col_bool.get(r);
        volatile std::int32_t vi = col_int.get(r);
        volatile std::uint32_t vu = col_uint.get(r);
        volatile std::int64_t vi64 = col_int64.get(r);
        volatile float vf = col_float.get(r);
        volatile double vd = col_double.get(r);
        auto vc = col_complex.get(r);
        auto vdc = col_dcomplex.get(r);
        std::string s = col_string.get(r);
        auto af = arr_float.get(r);
        auto ad = arr_double.get(r);
        (void)vb;
        (void)vi;
        (void)vu;
        (void)vi64;
        (void)vf;
        (void)vd;
        (void)vc;
        (void)vdc;
        (void)s;
        (void)af;
        (void)ad;
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

    try {
        auto table = cm::Table::open(table_path);
        std::cerr << "Opened table: " << table_path << " (" << table.nrow() << " rows)\n";

        double scalar_times[kPasses], array_times[kPasses], keyword_times[kPasses],
            row_times[kPasses];

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

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "TIMING scalar_seq_read " << scalar_med << "\n";
        std::cout << "TIMING array_seq_read " << array_med << "\n";
        std::cout << "TIMING keyword_access " << keyword_med << "\n";
        std::cout << "TIMING row_oriented_read " << row_med << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Cannot fully read table: " << table_path << " (" << e.what() << ")\n";
        std::cout << "TIMING scalar_seq_read N/A\n";
        std::cout << "TIMING array_seq_read N/A\n";
        std::cout << "TIMING keyword_access N/A\n";
        std::cout << "TIMING row_oriented_read N/A\n";
    }

    return 0;
}
