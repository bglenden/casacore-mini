#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

namespace {

fs::path temp_dir() {
    auto p = fs::temp_directory_path() / "casacore_mini_taql_agg_test";
    fs::create_directories(p);
    return p;
}

void cleanup(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

/// Table: CATEGORY(string), AMOUNT(double), FLAG(int)
/// 6 rows: 2 per category (A, B, C).
Table make_test_table(const fs::path& dir) {
    cleanup(dir);
    std::vector<TableColumnSpec> cols = {
        {"CATEGORY", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"AMOUNT", DataType::tp_double, ColumnKind::scalar, {}, ""},
        {"FLAG", DataType::tp_int, ColumnKind::scalar, {}, ""},
    };

    auto table = Table::create(dir, cols, 6);
    // A: 10, 20  B: 30, 40  C: 50, 60
    const char* cats[] = {"A", "A", "B", "B", "C", "C"};
    double amounts[] = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0};
    std::int32_t flags[] = {1, 0, 1, 1, 0, 0};

    for (int i = 0; i < 6; ++i) {
        table.write_scalar_cell("CATEGORY", static_cast<std::uint64_t>(i),
                                CellValue(std::string(cats[i])));
        table.write_scalar_cell("AMOUNT", static_cast<std::uint64_t>(i), CellValue(amounts[i]));
        table.write_scalar_cell("FLAG", static_cast<std::uint64_t>(i), CellValue(flags[i]));
    }
    table.flush();
    return table;
}

bool approx(double a, double b, double tol = 1e-9) {
    return std::abs(a - b) < tol;
}

// --- Ungrouped aggregates ---

void test_gcount() {
    auto dir = temp_dir() / "gcount";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT gcount() FROM t", table);
    assert(result.values.size() == 1);
    auto count = std::get<std::int64_t>(result.values[0]);
    (void)count;
    assert(count == 6);

    cleanup(dir);
}

void test_gsum() {
    auto dir = temp_dir() / "gsum";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT gsum(AMOUNT) FROM t", table);
    assert(result.values.size() == 1);
    auto sum = std::get<double>(result.values[0]);
    (void)sum;
    assert(approx(sum, 210.0));

    cleanup(dir);
}

void test_gmin_gmax() {
    auto dir = temp_dir() / "gminmax";
    auto table = make_test_table(dir);

    auto rmin = taql_execute("SELECT gmin(AMOUNT) FROM t", table);
    auto min_val = std::get<double>(rmin.values[0]);
    (void)min_val;
    assert(approx(min_val, 10.0));

    auto rmax = taql_execute("SELECT gmax(AMOUNT) FROM t", table);
    auto max_val = std::get<double>(rmax.values[0]);
    (void)max_val;
    assert(approx(max_val, 60.0));

    cleanup(dir);
}

void test_gmean() {
    auto dir = temp_dir() / "gmean";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT gmean(AMOUNT) FROM t", table);
    auto mean = std::get<double>(result.values[0]);
    (void)mean;
    assert(approx(mean, 35.0));

    cleanup(dir);
}

void test_gmedian() {
    auto dir = temp_dir() / "gmedian";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT gmedian(AMOUNT) FROM t", table);
    auto med = std::get<double>(result.values[0]);
    (void)med;
    // Sorted: 10, 20, 30, 40, 50, 60 -> median = (30+40)/2 = 35
    assert(approx(med, 35.0));

    cleanup(dir);
}

void test_gproduct() {
    auto dir = temp_dir() / "gproduct";
    auto table = make_test_table(dir);

    // Product of first 3 rows (A, A, B)
    auto result = taql_execute("SELECT gproduct(AMOUNT) FROM t WHERE AMOUNT <= 30", table);
    auto prod = std::get<double>(result.values[0]);
    (void)prod;
    assert(approx(prod, 6000.0)); // 10 * 20 * 30

    cleanup(dir);
}

// --- GROUPBY ---

void test_groupby_gcount() {
    auto dir = temp_dir() / "gb_gcount";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT CATEGORY, gcount() AS cnt FROM t GROUPBY CATEGORY", table);

    // 3 categories.
    assert(result.rows.size() == 3);
    assert(result.values.size() == 6); // 3 groups * 2 cols

    // Each category has 2 rows.
    // values: [cat, cnt, cat, cnt, cat, cnt]
    for (std::size_t i = 0; i < 3; ++i) {
        auto cnt = std::get<std::int64_t>(result.values[i * 2 + 1]);
        (void)cnt;
        assert(cnt == 2);
    }

    cleanup(dir);
}

void test_groupby_gsum() {
    auto dir = temp_dir() / "gb_gsum";
    auto table = make_test_table(dir);

    auto result =
        taql_execute("SELECT CATEGORY, gsum(AMOUNT) AS total FROM t GROUPBY CATEGORY", table);

    assert(result.rows.size() == 3);

    // Find the sums per category.
    // Groups should be A(30), B(70), C(110) in some order.
    double sums[3] = {};
    for (std::size_t i = 0; i < 3; ++i) {
        sums[i] = std::get<double>(result.values[i * 2 + 1]);
    }

    // Sort sums to verify.
    std::sort(std::begin(sums), std::end(sums));
    assert(approx(sums[0], 30.0));
    assert(approx(sums[1], 70.0));
    assert(approx(sums[2], 110.0));

    cleanup(dir);
}

// --- HAVING ---

void test_having_filter() {
    auto dir = temp_dir() / "having";
    auto table = make_test_table(dir);

    // Only groups where sum > 50.
    auto result = taql_execute("SELECT CATEGORY, gsum(AMOUNT) AS total FROM t "
                               "GROUPBY CATEGORY HAVING gsum(AMOUNT) > 50",
                               table);

    // B(70) and C(110) pass. A(30) is filtered.
    assert(result.rows.size() == 2);

    cleanup(dir);
}

void test_having_with_count() {
    auto dir = temp_dir() / "having_cnt";
    auto table = make_test_table(dir);

    // All groups have count == 2, so HAVING gcount() > 1 keeps all.
    auto result =
        taql_execute("SELECT CATEGORY FROM t GROUPBY CATEGORY HAVING gcount() > 1", table);
    assert(result.rows.size() == 3);

    // HAVING gcount() > 2 keeps none.
    auto result2 =
        taql_execute("SELECT CATEGORY FROM t GROUPBY CATEGORY HAVING gcount() > 2", table);
    assert(result2.rows.empty());

    cleanup(dir);
}

// --- Aggregate with WHERE ---

void test_aggregate_with_where() {
    auto dir = temp_dir() / "agg_where";
    auto table = make_test_table(dir);

    // Sum of amounts where FLAG = 1 (rows 0,2,3 = 10+30+40 = 80).
    auto result = taql_execute("SELECT gsum(AMOUNT) FROM t WHERE FLAG = 1", table);
    auto sum = std::get<double>(result.values[0]);
    (void)sum;
    assert(approx(sum, 80.0));

    cleanup(dir);
}

// --- Multiple aggregates ---

void test_multiple_aggregates() {
    auto dir = temp_dir() / "multi_agg";
    auto table = make_test_table(dir);

    auto result = taql_execute("SELECT gmin(AMOUNT), gmax(AMOUNT), gmean(AMOUNT) FROM t", table);
    assert(result.values.size() == 3);

    auto vmin = std::get<double>(result.values[0]);
    auto vmax = std::get<double>(result.values[1]);
    auto vmean = std::get<double>(result.values[2]);
    (void)vmin;
    (void)vmax;
    (void)vmean;
    assert(approx(vmin, 10.0));
    assert(approx(vmax, 60.0));
    assert(approx(vmean, 35.0));

    cleanup(dir);
}

} // namespace

int main() {
    test_gcount();
    test_gsum();
    test_gmin_gmax();
    test_gmean();
    test_gmedian();
    test_gproduct();
    test_groupby_gcount();
    test_groupby_gsum();
    test_having_filter();
    test_having_with_count();
    test_aggregate_with_where();
    test_multiple_aggregates();
    return 0;
}
