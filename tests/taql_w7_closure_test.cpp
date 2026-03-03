// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file taql_w7_closure_test.cpp
/// @brief W7 TaQL closure tests: new functions, regex, LIKE, rownr, end-to-end.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "FAIL: " << label << "\n";
    }
}

// ---------------------------------------------------------------------------
// New string functions via CALC
// ---------------------------------------------------------------------------

static void test_string_functions() {
    // LTRIM
    auto r = taql_calc("CALC LTRIM('  hello  ')");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "hello  ",
          "LTRIM removes leading spaces");

    // RTRIM
    r = taql_calc("CALC RTRIM('  hello  ')");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "  hello",
          "RTRIM removes trailing spaces");

    // CAPITALIZE
    r = taql_calc("CALC CAPITALIZE('hello world')");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "Hello World",
          "CAPITALIZE first letters");

    // SREVERSE
    r = taql_calc("CALC SREVERSE('hello')");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "olleh",
          "SREVERSE reverses string");

    // SUBSTR(str, start)
    r = taql_calc("CALC SUBSTR('hello world', 6)");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "world",
          "SUBSTR from position");

    // SUBSTR(str, start, len)
    r = taql_calc("CALC SUBSTR('hello world', 0, 5)");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "hello",
          "SUBSTR with length");

    // REPLACE(str, old, new)
    r = taql_calc("CALC REPLACE('hello world', 'world', 'earth')");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "hello earth",
          "REPLACE substitution");
}

// ---------------------------------------------------------------------------
// near/nearabs with 3 args
// ---------------------------------------------------------------------------

static void test_near_3arg() {
    auto r = taql_calc("CALC NEAR(1.0, 1.001, 0.01)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "NEAR 3-arg within tolerance");

    r = taql_calc("CALC NEAR(1.0, 2.0, 0.01)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == false,
          "NEAR 3-arg outside tolerance");
}

// ---------------------------------------------------------------------------
// Type conversion functions
// ---------------------------------------------------------------------------

static void test_type_conversion() {
    auto r = taql_calc("CALC BOOL(1)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "BOOL(1) is true");

    r = taql_calc("CALC BOOL(0)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == false,
          "BOOL(0) is false");

    r = taql_calc("CALC STRING(42)");
    check(!r.values.empty(), "STRING(42) returns value");
}

// ---------------------------------------------------------------------------
// Predicate functions: isnull, isdef
// ---------------------------------------------------------------------------

static void test_predicates() {
    auto r = taql_calc("CALC ISDEF(1.0)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "ISDEF of a value is true");

    r = taql_calc("CALC ISNULL(0)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == false,
          "ISNULL of non-null is false");
}

// ---------------------------------------------------------------------------
// RAND function
// ---------------------------------------------------------------------------

static void test_rand() {
    auto r = taql_calc("CALC RAND()");
    check(!r.values.empty(), "RAND returns a value");
    auto val = std::get<double>(r.values[0]);
    check(val >= 0.0 && val < 1.0, "RAND in [0,1)");
}

// ---------------------------------------------------------------------------
// LIKE with SQL patterns (%, _)
// ---------------------------------------------------------------------------

static void test_like_full() {
    auto r = taql_calc("CALC 'hello world' LIKE 'h%d'");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "LIKE h%d matches 'hello world'");

    r = taql_calc("CALC 'abc' LIKE 'a_c'");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "LIKE a_c matches 'abc'");

    r = taql_calc("CALC 'hello' LIKE '*llo'");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "LIKE *llo matches 'hello'");

    r = taql_calc("CALC 'test' LIKE 't?st'");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "LIKE t?st matches 'test'");

    r = taql_calc("CALC 'abc' NOT LIKE 'x%'");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true,
          "NOT LIKE x% for 'abc' is true");
}

// ---------------------------------------------------------------------------
// ROWNR() in table context
// ---------------------------------------------------------------------------

static void test_rownr_in_query() {
    auto path = fs::temp_directory_path() / "taql_w7_rownr";
    if (fs::exists(path)) fs::remove_all(path);

    // Create a simple table with 5 rows
    std::vector<TableColumnSpec> cols{{"VALUE", DataType::tp_double, ColumnKind::scalar, {}, "val"}};
    auto tbl = Table::create(path, cols, 5);
    for (std::uint64_t i = 0; i < 5; ++i) {
        tbl.write_scalar_cell("VALUE", i, CellValue{static_cast<double>(i) * 10.0});
    }
    tbl.flush();

    auto r = taql_execute("SELECT ROWNR() FROM t WHERE ROWNR() > 2", tbl);
    check(r.rows.size() == 2, "ROWNR() > 2 selects 2 rows");
    check(!r.rows.empty() && r.rows[0] == 3, "ROWNR() first matching row is 3");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// ISCOLUMN / ISKEYWORD in table context
// ---------------------------------------------------------------------------

static void test_iscolumn_iskeyword() {
    auto path = fs::temp_directory_path() / "taql_w7_iscol";
    if (fs::exists(path)) fs::remove_all(path);

    std::vector<TableColumnSpec> cols{{"COL_A", DataType::tp_double, ColumnKind::scalar, {}, "col"}};
    auto tbl = Table::create(path, cols, 1);
    tbl.write_scalar_cell("COL_A", 0, CellValue{1.0});
    tbl.flush();

    auto r = taql_execute("SELECT FROM t WHERE ISCOLUMN('COL_A')", tbl);
    check(r.rows.size() == 1, "ISCOLUMN('COL_A') is true");

    r = taql_execute("SELECT FROM t WHERE ISCOLUMN('COL_Z')", tbl);
    check(r.rows.empty(), "ISCOLUMN('COL_Z') is false");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// End-to-end: TaQL query over MS with MSSelection + WHERE
// ---------------------------------------------------------------------------

static MeasurementSet make_e2e_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    for (int i = 0; i < 4; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = "GROUND-BASED", .mount = "ALT-AZ",
                            .position = {}, .offset = {},
                            .dish_diameter = 25.0, .flag_row = false});
    }

    writer.add_field({.name = "SRC0", .code = "T", .time = 0.0,
                      .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_field({.name = "SRC1", .code = "T", .time = 0.0,
                      .num_poly = 0, .source_id = -1, .flag_row = false});

    writer.add_spectral_window({.num_chan = 64, .name = "SPW0",
        .ref_frequency = 1.4e9, .chan_freq = {}, .chan_width = {},
        .effective_bw = {}, .resolution = {}, .meas_freq_ref = 0,
        .total_bandwidth = 0.0, .net_sideband = 0, .if_conv_chain = 0,
        .freq_group = 0, .freq_group_name = {}, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0,
                                 .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA", .observer = "obs0",
        .project = {}, .release_date = 0.0, .flag_row = false});
    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0,
        .sub_scan = 0, .obs_mode = "OBSERVE_TARGET", .flag_row = false});

    struct Row { int a1, a2, fld, scan; double time; };
    Row rows[] = {
        {0, 1, 0, 1, 4.8e9},
        {0, 2, 0, 1, 4.8e9},
        {1, 2, 0, 2, 4.8e9 + 10},
        {0, 3, 1, 2, 4.8e9 + 10},
        {1, 3, 1, 3, 4.8e9 + 20},
        {2, 3, 0, 3, 4.8e9 + 20},
    };

    for (const auto& rd : rows) {
        writer.add_row({.antenna1 = rd.a1, .antenna2 = rd.a2, .array_id = 0,
                        .data_desc_id = 0, .exposure = 10.0, .feed1 = 0, .feed2 = 0,
                        .field_id = rd.fld, .flag_row = false, .interval = 10.0,
                        .observation_id = 0, .processor_id = 0, .scan_number = rd.scan,
                        .state_id = 0, .time = rd.time, .time_centroid = rd.time,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                        .data = {}, .flag = {}});
    }
    writer.flush();
    return MeasurementSet::open(path);
}

static void test_e2e_taql_over_ms() {
    auto path = fs::temp_directory_path() / "taql_w7_e2e";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_e2e_ms(path);

    // Use MSSelection to get WHERE, then execute TaQL over MS main table
    MsSelection sel;
    sel.set_scan_expr("1,2");
    sel.set_field_expr("0");

    auto where = sel.to_taql_where(ms);
    check(!where.empty(), "e2e: WHERE is non-empty");

    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);

    // Also evaluate directly for comparison
    auto eval_result = sel.evaluate(ms);

    check(taql_result.rows.size() == eval_result.rows.size(),
          "e2e: TaQL rows match evaluate rows");

    // scan 1,2 AND field 0: rows 0(scan1,fld0), 1(scan1,fld0), 2(scan2,fld0)
    check(taql_result.rows.size() == 3,
          "e2e: scan 1,2 + field 0 gives 3 rows");

    fs::remove_all(path);
}

static void test_e2e_mssel_with_taql_clause() {
    auto path = fs::temp_directory_path() / "taql_w7_e2e2";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_e2e_ms(path);

    // MSSelection with raw TaQL injection combined with category
    MsSelection sel;
    sel.set_scan_expr("1");
    sel.set_taql_expr("ANTENNA1 == 0");

    auto result = sel.evaluate(ms);
    // Scan 1 rows: (0,1,0,1) and (0,2,0,1). Both have ANTENNA1=0.
    check(result.rows.size() == 2,
          "e2e: scan 1 + TaQL ANTENNA1==0 gives 2 rows");

    // Also verify via to_taql_where
    auto where = sel.to_taql_where(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == result.rows.size(),
          "e2e: TaQL WHERE matches evaluate for combined MSSel+TaQL");

    fs::remove_all(path);
}

static void test_e2e_taql_count_over_ms() {
    auto path = fs::temp_directory_path() / "taql_w7_e2e3";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_e2e_ms(path);

    auto& main = ms.main_table();
    auto r = taql_execute("SELECT FROM t WHERE SCAN_NUMBER == 3", main);
    check(r.rows.size() == 2, "e2e: COUNT scan 3 gives 2 rows");

    auto cr = taql_execute("COUNT FROM t WHERE SCAN_NUMBER == 3", main);
    check(cr.affected_rows == 2, "e2e: COUNT command gives 2");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_string_functions();
    test_near_3arg();
    test_type_conversion();
    test_predicates();
    test_rand();
    test_like_full();
    test_rownr_in_query();
    test_iscolumn_iskeyword();
    test_e2e_taql_over_ms();
    test_e2e_mssel_with_taql_clause();
    test_e2e_taql_count_over_ms();

    std::cout << "taql_w7_closure_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
