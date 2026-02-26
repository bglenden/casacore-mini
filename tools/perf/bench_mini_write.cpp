// bench_mini_write.cpp -- casacore-mini: write benchmark timings.
//
// Built via CMake (bench_mini_write target).
//
// Usage: bench_mini_write <tmp_dir>
//   Creates fresh SSM tables in <tmp_dir>, reports write timings.

#include "casacore_mini/record.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_column.hpp"

#include <algorithm>
#include <chrono>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace cm = casacore_mini;
using Clock = std::chrono::steady_clock;

static constexpr std::uint64_t kNumRows = 40000;
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

// Data generators
static bool gen_bool(std::uint64_t row) { return (row % 2) == 0; }
static std::int32_t gen_int(std::uint64_t row) {
    return static_cast<std::int32_t>(row * 3 - 100000);
}
static std::uint32_t gen_uint(std::uint64_t row) {
    return static_cast<std::uint32_t>(row * 7);
}
static std::int64_t gen_int64(std::uint64_t row) {
    return static_cast<std::int64_t>(row) * 100003LL - 25000000000LL;
}
static float gen_float(std::uint64_t row) {
    return static_cast<float>(row) * 0.001F + 1.5F;
}
static double gen_double(std::uint64_t row) {
    return static_cast<double>(row) * 0.000001 + 3.14159265358979;
}
static std::complex<float> gen_complex(std::uint64_t row) {
    return {static_cast<float>(row) * 0.01F, static_cast<float>(row) * -0.02F};
}
static std::complex<double> gen_dcomplex(std::uint64_t row) {
    return {static_cast<double>(row) * 0.001, static_cast<double>(row) * -0.003};
}
static std::string gen_string(std::uint64_t row) {
    return "row_" + std::to_string(row) + "_value";
}

static std::vector<float> gen_arr_float(std::uint64_t row) {
    std::vector<float> v(kArrSize);
    for (int i = 0; i < kArrSize; ++i) {
        v[static_cast<std::size_t>(i)] = static_cast<float>(row * 256 + static_cast<std::uint64_t>(i)) * 0.01F;
    }
    return v;
}

static std::vector<double> gen_arr_double(std::uint64_t row) {
    std::vector<double> v(kArrSize);
    for (int i = 0; i < kArrSize; ++i) {
        v[static_cast<std::size_t>(i)] = static_cast<double>(row * 256 + static_cast<std::uint64_t>(i)) * 0.0001;
    }
    return v;
}

static void add_keywords(cm::Table& table) {
    auto& kw = table.rw_keywords();
    kw.set("VERSION", cm::RecordValue(std::string("1.0.0")));
    kw.set("TELESCOPE", cm::RecordValue(std::string("VLA")));
    kw.set("OBSERVER", cm::RecordValue(std::string("bench_test")));
    kw.set("PROJECT_ID", cm::RecordValue(std::string("BENCH-001")));
    kw.set("NUM_ROWS", cm::RecordValue(static_cast<std::int32_t>(kNumRows)));
    kw.set("TOTAL_TIME", cm::RecordValue(3600.0));
    kw.set("REF_FREQ", cm::RecordValue(1.4e9));
    kw.set("CHAN_WIDTH", cm::RecordValue(1e6));
    kw.set("NUM_CHAN", cm::RecordValue(std::int32_t{64}));
    kw.set("NUM_POL", cm::RecordValue(std::int32_t{4}));
    kw.set("IS_CALIBRATED", cm::RecordValue(false));
    kw.set("CREATOR", cm::RecordValue(std::string("bench_mini_write")));
    kw.set("FLOAT_PARAM", cm::RecordValue(2.718281828F));
    kw.set("COMPLEX_R", cm::RecordValue(1.0F));
    kw.set("COMPLEX_I", cm::RecordValue(-1.0F));
    kw.set("INT64_PARAM", cm::RecordValue(std::int64_t{9876543210LL}));
    kw.set("UINT_PARAM", cm::RecordValue(std::uint32_t{42}));

    cm::Record sub;
    sub.set("SUB_STR", cm::RecordValue(std::string("nested_value")));
    sub.set("SUB_INT", cm::RecordValue(std::int32_t{999}));
    sub.set("SUB_DOUBLE", cm::RecordValue(1.23456789));
    kw.set("SUB_RECORD", cm::RecordValue::from_record(std::move(sub)));

    cm::Record sub2;
    sub2.set("NAME", cm::RecordValue(std::string("second_sub")));
    sub2.set("VALUE", cm::RecordValue(42.0));
    kw.set("SUB_RECORD2", cm::RecordValue::from_record(std::move(sub2)));
}

// Full table columns spec
static std::vector<cm::TableColumnSpec> full_columns() {
    return {
        {.name = "sc_bool",     .data_type = cm::DataType::tp_bool},
        {.name = "sc_int",      .data_type = cm::DataType::tp_int},
        {.name = "sc_uint",     .data_type = cm::DataType::tp_uint},
        {.name = "sc_int64",    .data_type = cm::DataType::tp_int64},
        {.name = "sc_float",    .data_type = cm::DataType::tp_float},
        {.name = "sc_double",   .data_type = cm::DataType::tp_double},
        {.name = "sc_complex",  .data_type = cm::DataType::tp_complex},
        {.name = "sc_dcomplex", .data_type = cm::DataType::tp_dcomplex},
        {.name = "sc_string",   .data_type = cm::DataType::tp_string},
        {.name = "arr_float",   .data_type = cm::DataType::tp_float,
         .kind = cm::ColumnKind::array, .shape = {kArrDim0, kArrDim1}},
        {.name = "arr_double",  .data_type = cm::DataType::tp_double,
         .kind = cm::ColumnKind::array, .shape = {kArrDim0, kArrDim1}},
    };
}

static std::vector<cm::TableColumnSpec> scalar_columns() {
    return {
        {.name = "sc_bool",     .data_type = cm::DataType::tp_bool},
        {.name = "sc_int",      .data_type = cm::DataType::tp_int},
        {.name = "sc_uint",     .data_type = cm::DataType::tp_uint},
        {.name = "sc_int64",    .data_type = cm::DataType::tp_int64},
        {.name = "sc_float",    .data_type = cm::DataType::tp_float},
        {.name = "sc_double",   .data_type = cm::DataType::tp_double},
        {.name = "sc_complex",  .data_type = cm::DataType::tp_complex},
        {.name = "sc_dcomplex", .data_type = cm::DataType::tp_dcomplex},
        {.name = "sc_string",   .data_type = cm::DataType::tp_string},
    };
}

static std::vector<cm::TableColumnSpec> array_columns() {
    return {
        {.name = "arr_float",  .data_type = cm::DataType::tp_float,
         .kind = cm::ColumnKind::array, .shape = {kArrDim0, kArrDim1}},
        {.name = "arr_double", .data_type = cm::DataType::tp_double,
         .kind = cm::ColumnKind::array, .shape = {kArrDim0, kArrDim1}},
    };
}

static void write_full_table(const std::filesystem::path& path) {
    auto table = cm::Table::create(path, full_columns(), kNumRows);

    cm::ScalarColumn<bool> col_bool(table, "sc_bool");
    cm::ScalarColumn<std::int32_t> col_int(table, "sc_int");
    cm::ScalarColumn<std::uint32_t> col_uint(table, "sc_uint");
    cm::ScalarColumn<std::int64_t> col_int64(table, "sc_int64");
    cm::ScalarColumn<float> col_float(table, "sc_float");
    cm::ScalarColumn<double> col_double(table, "sc_double");
    cm::ScalarColumn<std::complex<float>> col_complex(table, "sc_complex");
    cm::ScalarColumn<std::complex<double>> col_dcomplex(table, "sc_dcomplex");
    cm::ScalarColumn<std::string> col_string(table, "sc_string");
    cm::ArrayColumn<float> col_arr_float(table, "arr_float");
    cm::ArrayColumn<double> col_arr_double(table, "arr_double");

    for (std::uint64_t r = 0; r < kNumRows; ++r) {
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

// Full table write: create+fill+destroy x kFullRepeats
static double bench_full_write(const std::filesystem::path& dir) {
    auto t0 = Clock::now();
    for (int rep = 0; rep < kFullRepeats; ++rep) {
        auto path = dir / ("mini_full_write_" + std::to_string(rep));
        std::filesystem::remove_all(path);
        write_full_table(path);
        std::filesystem::remove_all(path);
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// Scalar-only write: create+fill+destroy x kScalarRepeats
static double bench_scalar_write(const std::filesystem::path& dir) {
    auto t0 = Clock::now();
    for (int rep = 0; rep < kScalarRepeats; ++rep) {
        auto path = dir / ("mini_scalar_write_" + std::to_string(rep));
        std::filesystem::remove_all(path);

        auto table = cm::Table::create(path, scalar_columns(), kNumRows);

        cm::ScalarColumn<bool> col_bool(table, "sc_bool");
        cm::ScalarColumn<std::int32_t> col_int(table, "sc_int");
        cm::ScalarColumn<std::uint32_t> col_uint(table, "sc_uint");
        cm::ScalarColumn<std::int64_t> col_int64(table, "sc_int64");
        cm::ScalarColumn<float> col_float(table, "sc_float");
        cm::ScalarColumn<double> col_double(table, "sc_double");
        cm::ScalarColumn<std::complex<float>> col_complex(table, "sc_complex");
        cm::ScalarColumn<std::complex<double>> col_dcomplex(table, "sc_dcomplex");
        cm::ScalarColumn<std::string> col_string(table, "sc_string");

        for (std::uint64_t r = 0; r < kNumRows; ++r) {
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
        // table destructs here before remove_all
        std::filesystem::remove_all(path);
    }
    auto t1 = Clock::now();
    return elapsed_sec(t0, t1);
}

// Array-only write: create+fill+destroy x kArrayRepeats
static double bench_array_write(const std::filesystem::path& dir) {
    auto t0 = Clock::now();
    for (int rep = 0; rep < kArrayRepeats; ++rep) {
        auto path = dir / ("mini_array_write_" + std::to_string(rep));
        std::filesystem::remove_all(path);

        auto table = cm::Table::create(path, array_columns(), kNumRows);

        cm::ArrayColumn<float> col_arr_float(table, "arr_float");
        cm::ArrayColumn<double> col_arr_double(table, "arr_double");

        for (std::uint64_t r = 0; r < kNumRows; ++r) {
            col_arr_float.put(r, gen_arr_float(r));
            col_arr_double.put(r, gen_arr_double(r));
        }
        table.flush();

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
    std::cerr << "Write benchmark: " << dir << " (" << kNumRows << " rows)\n";

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
