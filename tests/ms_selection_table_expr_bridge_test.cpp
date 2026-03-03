// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file ms_selection_table_expr_bridge_test.cpp
/// @brief Tests for to_taql_where() bridge and accessor methods.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"
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

/// Build test MS with 4 antennas, 2 fields, 1 SPW, 3 scans, 2 obs, 2 arrays, 12 rows.
static MeasurementSet make_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    for (int i = 0; i < 4; ++i) {
        writer.add_antenna({.name = "A" + std::to_string(i),
                            .station = "P" + std::to_string(i),
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
    }

    writer.add_field({.name = "SRC0", .code = "T", .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_field({.name = "SRC1", .code = "T", .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});

    writer.add_spectral_window({.num_chan = 64, .name = "SPW0", .ref_frequency = 1.4e9,
                                .chan_freq = {}, .chan_width = {}, .effective_bw = {}, .resolution = {},
                                .meas_freq_ref = 0, .total_bandwidth = 0.0, .net_sideband = 0,
                                .if_conv_chain = 0, .freq_group = 0, .freq_group_name = {}, .flag_row = false});

    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

    writer.add_observation({.telescope_name = "VLA", .observer = "obs0", .project = {}, .release_date = 0.0, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA", .observer = "obs1", .project = {}, .release_date = 0.0, .flag_row = false});

    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0, .sub_scan = 0, .obs_mode = "OBSERVE_TARGET", .flag_row = false});
    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0, .sub_scan = 0, .obs_mode = "CALIBRATE_BANDPASS", .flag_row = false});

    struct Row { int a1, a2, fld, scan, state, obs, arr, f1, f2; double time; };
    Row rows[] = {
        {0, 1, 0, 1, 0, 0, 0, 0, 0, 4.8e9},
        {0, 2, 0, 1, 0, 0, 0, 0, 1, 4.8e9},
        {1, 2, 0, 1, 0, 0, 1, 0, 0, 4.8e9},
        {0, 3, 0, 2, 0, 0, 1, 1, 1, 4.8e9 + 10},
        {0, 1, 1, 2, 1, 1, 0, 0, 1, 4.8e9 + 20},
        {0, 2, 1, 2, 1, 1, 0, 0, 0, 4.8e9 + 20},
        {1, 3, 1, 3, 1, 1, 1, 1, 0, 4.8e9 + 30},
        {2, 3, 0, 3, 0, 0, 0, 0, 0, 4.8e9 + 30},
        {0, 1, 0, 3, 0, 0, 1, 0, 0, 4.8e9 + 40},
        {0, 0, 0, 1, 0, 0, 0, 0, 0, 4.8e9},
        {1, 1, 1, 2, 1, 1, 0, 1, 1, 4.8e9 + 20},
        {2, 3, 1, 3, 1, 1, 1, 0, 1, 4.8e9 + 40},
    };

    for (const auto& rd : rows) {
        writer.add_row({.antenna1 = rd.a1, .antenna2 = rd.a2, .array_id = rd.arr,
                        .data_desc_id = 0, .exposure = 10.0, .feed1 = rd.f1, .feed2 = rd.f2,
                        .field_id = rd.fld, .flag_row = false, .interval = 10.0,
                        .observation_id = rd.obs, .processor_id = 0, .scan_number = rd.scan,
                        .state_id = rd.state, .time = rd.time, .time_centroid = rd.time,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                        .data = {}, .flag = {}});
    }
    writer.flush();
    return MeasurementSet::open(path);
}

// ---------------------------------------------------------------------------
// to_taql_where basic tests
// ---------------------------------------------------------------------------

static void test_empty_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_empty";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    auto where = sel.to_taql_where(ms);
    check(where.empty(), "no selection => empty WHERE");

    fs::remove_all(path);
}

static void test_scan_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_scan";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr("1,2");
    auto where = sel.to_taql_where(ms);
    check(where.find("SCAN_NUMBER") != std::string::npos, "scan WHERE has SCAN_NUMBER");
    check(where.find("IN") != std::string::npos, "scan WHERE has IN");

    // Execute via TaQL and compare with evaluate
    auto eval_result = sel.evaluate(ms);
    auto query = "SELECT FROM t WHERE " + where;
    auto& main = ms.main_table();
    auto taql_result = taql_execute(query, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE scan rows match evaluate rows");

    fs::remove_all(path);
}

static void test_field_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_field";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("0");
    auto where = sel.to_taql_where(ms);
    check(where.find("FIELD_ID") != std::string::npos, "field WHERE has FIELD_ID");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE field rows match evaluate rows");

    fs::remove_all(path);
}

static void test_observation_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_obs";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr("1");
    auto where = sel.to_taql_where(ms);
    check(where.find("OBSERVATION_ID") != std::string::npos, "obs WHERE has OBSERVATION_ID");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE obs rows match evaluate rows");

    fs::remove_all(path);
}

static void test_array_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_arr";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_array_expr("0");
    auto where = sel.to_taql_where(ms);
    check(where.find("ARRAY_ID") != std::string::npos, "arr WHERE has ARRAY_ID");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE arr rows match evaluate rows");

    fs::remove_all(path);
}

static void test_antenna_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_ant";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0,1");
    auto where = sel.to_taql_where(ms);
    check(where.find("ANTENNA") != std::string::npos, "ant WHERE has ANTENNA");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE antenna rows match evaluate rows");

    fs::remove_all(path);
}

static void test_combined_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_comb";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr("1");
    sel.set_field_expr("0");
    auto where = sel.to_taql_where(ms);
    check(where.find("AND") != std::string::npos, "combined WHERE has AND");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE combined rows match evaluate rows");

    fs::remove_all(path);
}

static void test_state_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_state";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_state_expr("0");
    auto where = sel.to_taql_where(ms);
    check(where.find("STATE_ID") != std::string::npos, "state WHERE has STATE_ID");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE state rows match evaluate rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Accessor tests
// ---------------------------------------------------------------------------

static void test_accessors() {
    MsSelection sel;
    check(!sel.antenna_expr().has_value(), "antenna_expr empty initially");

    sel.set_antenna_expr("0,1");
    check(sel.antenna_expr().has_value(), "antenna_expr has value after set");
    check(*sel.antenna_expr() == "0,1", "antenna_expr value correct");

    sel.set_observation_expr("0");
    check(*sel.observation_expr() == "0", "observation_expr value");

    sel.set_taql_expr("SCAN_NUMBER > 1");
    check(*sel.taql_expr() == "SCAN_NUMBER > 1", "taql_expr value");

    sel.clear();
    check(!sel.antenna_expr().has_value(), "cleared antenna_expr");
    check(!sel.observation_expr().has_value(), "cleared observation_expr");
    check(!sel.taql_expr().has_value(), "cleared taql_expr");
}

static void test_scan_bounds_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_scanbnd";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr(">2");
    auto where = sel.to_taql_where(ms);
    check(where.find("SCAN_NUMBER > 2") != std::string::npos, "scan bound WHERE");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE scan bound rows match evaluate rows");

    fs::remove_all(path);
}

static void test_taql_injection_where() {
    auto path = fs::temp_directory_path() / "mssel_w6_taql";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_taql_expr("SCAN_NUMBER > 1");
    auto where = sel.to_taql_where(ms);
    check(where.find("SCAN_NUMBER > 1") != std::string::npos, "TaQL injection in WHERE");

    auto eval_result = sel.evaluate(ms);
    auto& main = ms.main_table();
    auto taql_result = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_result.rows.size() == eval_result.rows.size(),
          "TaQL WHERE injection rows match evaluate rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_empty_where();
    test_scan_where();
    test_field_where();
    test_observation_where();
    test_array_where();
    test_antenna_where();
    test_combined_where();
    test_state_where();
    test_accessors();
    test_scan_bounds_where();
    test_taql_injection_where();

    std::cout << "ms_selection_table_expr_bridge_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
