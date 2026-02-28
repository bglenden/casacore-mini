#include "casacore_mini/taql.hpp"
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

static double as_dbl(const TaqlValue& v) {
    if (auto* d = std::get_if<double>(&v)) return *d;
    if (auto* i = std::get_if<std::int64_t>(&v)) return static_cast<double>(*i);
    throw std::runtime_error("not a number");
}

static std::int64_t as_i64(const TaqlValue& v) {
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i;
    throw std::runtime_error("not an int64");
}

static bool as_bl(const TaqlValue& v) {
    if (auto* b = std::get_if<bool>(&v)) return *b;
    throw std::runtime_error("not a bool");
}

// ---- Array info functions ----
static void test_ndim_nelem_shape() {
    {
        auto r = taql_calc("CALC NDIM([1,2,3])");
        check(as_i64(r.values[0]) == 1, "ndim of array == 1");
    }
    {
        auto r = taql_calc("CALC NDIM(42)");
        check(as_i64(r.values[0]) == 0, "ndim of scalar == 0");
    }
    {
        auto r = taql_calc("CALC NELEM([10,20,30,40])");
        check(as_i64(r.values[0]) == 4, "nelem of 4-elem array");
    }
    {
        auto r = taql_calc("CALC SHAPE([1,2,3])");
        auto* arr = std::get_if<std::vector<std::int64_t>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 1 && (*arr)[0] == 3, "shape of 3-elem array");
    }
}

// ---- Array reductions ----
static void test_array_reductions() {
    {
        auto r = taql_calc("CALC ARRSUM([1,2,3,4,5])");
        check(std::abs(as_dbl(r.values[0]) - 15.0) < 1e-10, "arrsum");
    }
    {
        auto r = taql_calc("CALC ARRMIN([5,3,7,1,4])");
        check(std::abs(as_dbl(r.values[0]) - 1.0) < 1e-10, "arrmin");
    }
    {
        auto r = taql_calc("CALC ARRMAX([5,3,7,1,4])");
        check(std::abs(as_dbl(r.values[0]) - 7.0) < 1e-10, "arrmax");
    }
    {
        auto r = taql_calc("CALC ARRMEAN([2,4,6])");
        check(std::abs(as_dbl(r.values[0]) - 4.0) < 1e-10, "arrmean");
    }
    {
        auto r = taql_calc("CALC ARRMEDIAN([1,3,5,7])");
        check(std::abs(as_dbl(r.values[0]) - 4.0) < 1e-10, "arrmedian even");
    }
    {
        auto r = taql_calc("CALC ARRMEDIAN([1,3,5])");
        check(std::abs(as_dbl(r.values[0]) - 3.0) < 1e-10, "arrmedian odd");
    }
    {
        auto r = taql_calc("CALC ARRVARIANCE([2,4,4,4,5,5,7,9])");
        // sample variance = 4.571...
        check(std::abs(as_dbl(r.values[0]) - 4.571428571428571) < 1e-5, "arrvariance");
    }
    {
        auto r = taql_calc("CALC ARRSTDDEV([2,4,4,4,5,5,7,9])");
        check(std::abs(as_dbl(r.values[0]) - std::sqrt(4.571428571428571)) < 1e-5, "arrstddev");
    }
    {
        auto r = taql_calc("CALC ARRRMS([3,4])");
        // rms = sqrt((9+16)/2) = sqrt(12.5) = 3.5355...
        check(std::abs(as_dbl(r.values[0]) - std::sqrt(12.5)) < 1e-5, "arrrms");
    }
    {
        auto r = taql_calc("CALC ARRFRACTILE([1,2,3,4,5], 0.5)");
        check(std::abs(as_dbl(r.values[0]) - 3.0) < 1e-10, "arrfractile 50%");
    }
}

// ---- Array boolean reductions ----
static void test_array_boolean_reductions() {
    {
        auto r = taql_calc("CALC ARRANY([0,0,1])");
        check(as_bl(r.values[0]) == true, "arrany with a true");
    }
    {
        auto r = taql_calc("CALC ARRANY([0,0,0])");
        check(as_bl(r.values[0]) == false, "arrany all false");
    }
    {
        auto r = taql_calc("CALC ARRALL([1,1,1])");
        check(as_bl(r.values[0]) == true, "arrall all true");
    }
    {
        auto r = taql_calc("CALC ARRALL([1,0,1])");
        check(as_bl(r.values[0]) == false, "arrall with false");
    }
    {
        auto r = taql_calc("CALC ARRNTRUE([1,0,1,1,0])");
        check(as_i64(r.values[0]) == 3, "arrntrue");
    }
    {
        auto r = taql_calc("CALC ARRNFALSE([1,0,1,1,0])");
        check(as_i64(r.values[0]) == 2, "arrnfalse");
    }
}

// ---- Array manipulation ----
static void test_array_manipulation() {
    {
        auto r = taql_calc("CALC ARRAY(7.0, 3)");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 3, "array construction size");
        check(arr != nullptr && (*arr)[0] == 7.0 && (*arr)[2] == 7.0, "array fill value");
    }
    {
        auto r = taql_calc("CALC AREVERSE([1,2,3])");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 3, "areverse size");
        check(arr != nullptr && (*arr)[0] == 3.0 && (*arr)[2] == 1.0, "areverse values");
    }
    {
        auto r = taql_calc("CALC RESIZE([1,2,3], 5)");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 5, "resize size");
        check(arr != nullptr && (*arr)[3] == 0.0, "resize fill");
    }
    {
        // TRANSPOSE of 1-D is identity
        auto r = taql_calc("CALC TRANSPOSE([1,2,3])");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 3, "transpose 1D identity");
    }
}

// ---- Running/boxed reductions ----
static void test_running_reductions() {
    {
        auto r = taql_calc("CALC RUNNINGSUM([1,2,3,4], 2)");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && arr->size() == 4, "runningsum size");
        // Window 2: [1], [1+2], [2+3], [3+4]
        check(arr != nullptr && std::abs((*arr)[0] - 1.0) < 1e-10, "runningsum[0]");
        check(arr != nullptr && std::abs((*arr)[1] - 3.0) < 1e-10, "runningsum[1]");
        check(arr != nullptr && std::abs((*arr)[3] - 7.0) < 1e-10, "runningsum[3]");
    }
    {
        auto r = taql_calc("CALC RUNNINGMEAN([2,4,6,8], 2)");
        auto* arr = std::get_if<std::vector<double>>(&r.values[0]);
        check(arr != nullptr && std::abs((*arr)[1] - 3.0) < 1e-10, "runningmean[1]");
        check(arr != nullptr && std::abs((*arr)[3] - 7.0) < 1e-10, "runningmean[3]");
    }
}

// ---- Array column reading (using a real table) ----
static void test_array_column_read() {
    auto tmp = fs::temp_directory_path() / "taql_array_test";
    fs::remove_all(tmp);

    // Create table with an array column
    std::vector<TableColumnSpec> cols = {
        {"vals", DataType::tp_double, ColumnKind::array, {3}, ""},
        {"id", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 2);
    table.write_array_double_cell("vals", 0, {10.0, 20.0, 30.0});
    table.write_array_double_cell("vals", 1, {40.0, 50.0, 60.0});
    table.write_scalar_cell("id", 0, std::int32_t{1});
    table.write_scalar_cell("id", 1, std::int32_t{2});
    table.flush();

    // Test CALC with array functions over table columns
    auto r = taql_execute("SELECT ARRSUM(vals) AS s FROM t", table);
    check(r.rows.size() == 2, "array column: 2 rows selected");
    check(std::abs(as_dbl(r.values[0]) - 60.0) < 1e-10, "arrsum(vals) row 0");
    check(std::abs(as_dbl(r.values[1]) - 150.0) < 1e-10, "arrsum(vals) row 1");

    // Test ARRMEAN
    auto r2 = taql_execute("SELECT ARRMEAN(vals) AS m FROM t", table);
    check(std::abs(as_dbl(r2.values[0]) - 20.0) < 1e-10, "arrmean(vals) row 0");

    // Test NELEM on column
    auto r3 = taql_execute("SELECT NELEM(vals) AS n FROM t", table);
    check(as_i64(r3.values[0]) == 3, "nelem(vals) == 3");

    fs::remove_all(tmp);
}

int main() {
    test_ndim_nelem_shape();
    test_array_reductions();
    test_array_boolean_reductions();
    test_array_manipulation();
    test_running_reductions();
    test_array_column_read();

    std::cout << "taql_array_eval_test: " << check_count << " checks passed\n";
    return 0;
}
