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

// Create a test table with groups:
//   category  val
//   A         10
//   A         20
//   A         30
//   B         40
//   B         50
static std::pair<fs::path, Table> make_grouped_table() {
    auto tmp = fs::temp_directory_path() / "taql_aggregate_test";
    fs::remove_all(tmp);

    std::vector<TableColumnSpec> cols = {
        {"category", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"val", DataType::tp_double, ColumnKind::scalar, {}, ""},
        {"flag", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };
    auto table = Table::create(tmp, cols, 5);
    table.write_scalar_cell("category", 0, std::string("A"));
    table.write_scalar_cell("category", 1, std::string("A"));
    table.write_scalar_cell("category", 2, std::string("A"));
    table.write_scalar_cell("category", 3, std::string("B"));
    table.write_scalar_cell("category", 4, std::string("B"));
    table.write_scalar_cell("val", 0, 10.0);
    table.write_scalar_cell("val", 1, 20.0);
    table.write_scalar_cell("val", 2, 30.0);
    table.write_scalar_cell("val", 3, 40.0);
    table.write_scalar_cell("val", 4, 50.0);
    table.write_scalar_cell("flag", 0, std::int32_t{1});
    table.write_scalar_cell("flag", 1, std::int32_t{0});
    table.write_scalar_cell("flag", 2, std::int32_t{1});
    table.write_scalar_cell("flag", 3, std::int32_t{1});
    table.write_scalar_cell("flag", 4, std::int32_t{1});
    table.flush();
    return {tmp, std::move(table)};
}

// ---- GROUPBY with GCOUNT ----
static void test_groupby_count() {
    auto [tmp, table] = make_grouped_table();

    auto r = taql_execute("SELECT category, GCOUNT() AS cnt FROM t GROUPBY category", table);
    // Should have 2 groups: A (3 rows), B (2 rows)
    check(r.rows.size() == 2, "groupby count: 2 groups");
    // Values: category_A, 3, category_B, 2 (2 columns per group)
    check(r.values.size() == 4, "groupby count: 4 values (2 groups x 2 cols)");

    // Find which group is A and which is B
    auto* cat0 = std::get_if<std::string>(&r.values[0]);
    auto* cat1 = std::get_if<std::string>(&r.values[2]);
    check(cat0 != nullptr && cat1 != nullptr, "groupby count: categories are strings");

    if (*cat0 == "A") {
        check(as_i64(r.values[1]) == 3, "groupby count: A has 3");
        check(as_i64(r.values[3]) == 2, "groupby count: B has 2");
    } else {
        check(as_i64(r.values[1]) == 2, "groupby count: B has 2");
        check(as_i64(r.values[3]) == 3, "groupby count: A has 3");
    }

    fs::remove_all(tmp);
}

// ---- GROUPBY with GSUM, GMEAN ----
static void test_groupby_sum_mean() {
    auto [tmp, table] = make_grouped_table();

    auto r = taql_execute(
        "SELECT category, GSUM(val) AS s, GMEAN(val) AS m FROM t GROUPBY category", table);
    check(r.rows.size() == 2, "groupby sum/mean: 2 groups");
    // 3 columns per row: category, sum, mean

    auto* cat0 = std::get_if<std::string>(&r.values[0]);
    if (cat0 != nullptr && *cat0 == "A") {
        check(std::abs(as_dbl(r.values[1]) - 60.0) < 1e-10, "sum A = 60");
        check(std::abs(as_dbl(r.values[2]) - 20.0) < 1e-10, "mean A = 20");
        check(std::abs(as_dbl(r.values[4]) - 90.0) < 1e-10, "sum B = 90");
        check(std::abs(as_dbl(r.values[5]) - 45.0) < 1e-10, "mean B = 45");
    } else {
        check(std::abs(as_dbl(r.values[1]) - 90.0) < 1e-10, "sum B = 90");
        check(std::abs(as_dbl(r.values[2]) - 45.0) < 1e-10, "mean B = 45");
        check(std::abs(as_dbl(r.values[4]) - 60.0) < 1e-10, "sum A = 60");
        check(std::abs(as_dbl(r.values[5]) - 20.0) < 1e-10, "mean A = 20");
    }

    fs::remove_all(tmp);
}

// ---- GROUPBY with GMIN, GMAX ----
static void test_groupby_min_max() {
    auto [tmp, table] = make_grouped_table();

    auto r = taql_execute(
        "SELECT GMIN(val) AS mn, GMAX(val) AS mx FROM t GROUPBY category", table);
    check(r.rows.size() == 2, "groupby min/max: 2 groups");

    // Group A: min=10, max=30; Group B: min=40, max=50
    // Values come in group order (A first since it's encountered first)
    double mn0 = as_dbl(r.values[0]);
    double mx0 = as_dbl(r.values[1]);
    double mn1 = as_dbl(r.values[2]);
    double mx1 = as_dbl(r.values[3]);

    // Either order
    if (mn0 < 20) { // Group A first
        check(std::abs(mn0 - 10.0) < 1e-10, "min A = 10");
        check(std::abs(mx0 - 30.0) < 1e-10, "max A = 30");
        check(std::abs(mn1 - 40.0) < 1e-10, "min B = 40");
        check(std::abs(mx1 - 50.0) < 1e-10, "max B = 50");
    } else {
        check(std::abs(mn0 - 40.0) < 1e-10, "min B = 40");
        check(std::abs(mx0 - 50.0) < 1e-10, "max B = 50");
        check(std::abs(mn1 - 10.0) < 1e-10, "min A = 10");
        check(std::abs(mx1 - 30.0) < 1e-10, "max A = 30");
    }

    fs::remove_all(tmp);
}

// ---- HAVING filter ----
static void test_having() {
    auto [tmp, table] = make_grouped_table();

    auto r = taql_execute(
        "SELECT category, GCOUNT() AS cnt FROM t GROUPBY category HAVING GCOUNT() > 2", table);
    // Only group A has count > 2
    check(r.rows.size() == 1, "having: 1 group passes");
    auto* cat = std::get_if<std::string>(&r.values[0]);
    check(cat != nullptr && *cat == "A", "having: group A passes");
    check(as_i64(r.values[1]) == 3, "having: count = 3");

    fs::remove_all(tmp);
}

// ---- GANY, GALL, GNTRUE, GNFALSE ----
static void test_boolean_aggregates() {
    auto [tmp, table] = make_grouped_table();

    // flag: A=[1,0,1], B=[1,1]
    auto r = taql_execute(
        "SELECT GANY(flag) AS a, GALL(flag) AS al, GNTRUE(flag) AS nt, GNFALSE(flag) AS nf "
        "FROM t GROUPBY category", table);

    check(r.rows.size() == 2, "bool agg: 2 groups");

    // Group A: any=true, all=false, ntrue=2, nfalse=1
    // Group B: any=true, all=true, ntrue=2, nfalse=0
    bool any0 = as_bl(r.values[0]);
    bool all0 = as_bl(r.values[1]);
    auto nt0 = as_i64(r.values[2]);
    auto nf0 = as_i64(r.values[3]);

    bool any1 = as_bl(r.values[4]);
    bool all1 = as_bl(r.values[5]);
    auto nt1 = as_i64(r.values[6]);
    auto nf1 = as_i64(r.values[7]);

    if (nf0 == 1) { // Group A first
        check(any0 == true, "A gany=true");
        check(all0 == false, "A gall=false");
        check(nt0 == 2, "A gntrue=2");
        check(nf0 == 1, "A gnfalse=1");
        check(any1 == true, "B gany=true");
        check(all1 == true, "B gall=true");
        check(nt1 == 2, "B gntrue=2");
        check(nf1 == 0, "B gnfalse=0");
    } else {
        check(any0 == true, "B gany=true");
        check(all0 == true, "B gall=true");
        check(any1 == true, "A gany=true");
        check(all1 == false, "A gall=false");
    }

    fs::remove_all(tmp);
}

// ---- GMEDIAN ----
static void test_gmedian() {
    auto [tmp, table] = make_grouped_table();

    auto r = taql_execute(
        "SELECT GMEDIAN(val) AS med FROM t GROUPBY category", table);
    check(r.rows.size() == 2, "gmedian: 2 groups");

    double med0 = as_dbl(r.values[0]);
    double med1 = as_dbl(r.values[1]);

    // Group A: median of [10,20,30] = 20
    // Group B: median of [40,50] = 45
    if (med0 < 25) {
        check(std::abs(med0 - 20.0) < 1e-10, "median A = 20");
        check(std::abs(med1 - 45.0) < 1e-10, "median B = 45");
    } else {
        check(std::abs(med0 - 45.0) < 1e-10, "median B = 45");
        check(std::abs(med1 - 20.0) < 1e-10, "median A = 20");
    }

    fs::remove_all(tmp);
}

// ---- WHERE + GROUPBY ----
static void test_where_groupby() {
    auto [tmp, table] = make_grouped_table();

    // Filter val > 15, then group
    auto r = taql_execute(
        "SELECT category, GCOUNT() AS cnt FROM t WHERE val > 15 GROUPBY category", table);

    // After WHERE: A=[20,30], B=[40,50]
    check(r.rows.size() == 2, "where+groupby: 2 groups");
    auto* cat0 = std::get_if<std::string>(&r.values[0]);
    if (cat0 != nullptr && *cat0 == "A") {
        check(as_i64(r.values[1]) == 2, "where+groupby: A count=2");
        check(as_i64(r.values[3]) == 2, "where+groupby: B count=2");
    } else {
        check(as_i64(r.values[1]) == 2, "where+groupby: B count=2");
        check(as_i64(r.values[3]) == 2, "where+groupby: A count=2");
    }

    fs::remove_all(tmp);
}

int main() {
    test_groupby_count();
    test_groupby_sum_mean();
    test_groupby_min_max();
    test_having();
    test_boolean_aggregates();
    test_gmedian();
    test_where_groupby();

    std::cout << "taql_aggregate_test: " << check_count << " checks passed\n";
    return 0;
}
