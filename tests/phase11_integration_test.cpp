// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file phase11_integration_test.cpp
/// @brief P11-W8 cross-tranche integration test: MS creation -> TaQL queries -> MSSelection.

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

/// Build a realistic MS with multiple antennas, fields, scans, observations.
static MeasurementSet make_integration_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // 6 antennas
    for (int i = 0; i < 6; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = "GROUND-BASED", .mount = "ALT-AZ",
                            .position = {}, .offset = {},
                            .dish_diameter = 25.0, .flag_row = false});
    }

    // 3 fields
    for (int i = 0; i < 3; ++i) {
        writer.add_field({.name = "SRC" + std::to_string(i), .code = "T",
                          .time = 0.0, .num_poly = 0, .source_id = -1,
                          .flag_row = false});
    }

    writer.add_spectral_window({.num_chan = 64, .name = "SPW0",
        .ref_frequency = 1.4e9, .chan_freq = {}, .chan_width = {},
        .effective_bw = {}, .resolution = {}, .meas_freq_ref = 0,
        .total_bandwidth = 0.0, .net_sideband = 0, .if_conv_chain = 0,
        .freq_group = 0, .freq_group_name = {}, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0,
                                 .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

    for (int i = 0; i < 2; ++i) {
        writer.add_observation({.telescope_name = "VLA",
            .observer = "obs" + std::to_string(i),
            .project = {}, .release_date = 0.0, .flag_row = false});
    }

    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0,
        .sub_scan = 0, .obs_mode = "OBSERVE_TARGET", .flag_row = false});

    // 30 rows across 5 scans, 3 fields, varying antennas
    struct Row { int a1, a2, fld, scan, obs; double time; };
    Row rows[] = {
        {0,1,0,1,0,4.8e9},     {0,2,0,1,0,4.8e9},     {1,2,0,1,0,4.8e9},
        {0,3,0,1,0,4.8e9},     {1,3,0,1,0,4.8e9},     {2,3,0,1,0,4.8e9},
        {0,1,1,2,0,4.8e9+10},  {0,2,1,2,0,4.8e9+10},  {1,2,1,2,0,4.8e9+10},
        {3,4,1,2,0,4.8e9+10},  {3,5,1,2,0,4.8e9+10},  {4,5,1,2,0,4.8e9+10},
        {0,1,2,3,1,4.8e9+20},  {0,2,2,3,1,4.8e9+20},  {0,3,2,3,1,4.8e9+20},
        {1,4,2,3,1,4.8e9+20},  {2,5,2,3,1,4.8e9+20},  {3,4,2,3,1,4.8e9+20},
        {0,1,0,4,1,4.8e9+30},  {0,5,0,4,1,4.8e9+30},  {1,5,0,4,1,4.8e9+30},
        {2,3,0,4,1,4.8e9+30},  {2,4,0,4,1,4.8e9+30},  {3,5,0,4,1,4.8e9+30},
        {0,1,1,5,0,4.8e9+40},  {0,3,1,5,0,4.8e9+40},  {1,2,1,5,0,4.8e9+40},
        {2,5,1,5,0,4.8e9+40},  {3,4,1,5,0,4.8e9+40},  {4,5,1,5,0,4.8e9+40},
    };

    for (const auto& rd : rows) {
        writer.add_row({.antenna1 = rd.a1, .antenna2 = rd.a2, .array_id = 0,
                        .data_desc_id = 0, .exposure = 10.0, .feed1 = 0, .feed2 = 0,
                        .field_id = rd.fld, .flag_row = false, .interval = 10.0,
                        .observation_id = rd.obs, .processor_id = 0, .scan_number = rd.scan,
                        .state_id = 0, .time = rd.time, .time_centroid = rd.time,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                        .data = {}, .flag = {}});
    }
    writer.flush();
    return MeasurementSet::open(path);
}

// ---------------------------------------------------------------------------
// Test: Full MS pipeline — create, query via TaQL, filter via MSSelection
// ---------------------------------------------------------------------------

static void test_full_pipeline() {
    auto path = fs::temp_directory_path() / "p11_integ";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_integration_ms(path);
    auto& main = ms.main_table();

    check(main.nrow() == 30, "MS has 30 rows");

    // Direct TaQL queries
    auto r1 = taql_execute("SELECT FROM t WHERE SCAN_NUMBER == 1", main);
    check(r1.rows.size() == 6, "TaQL: scan 1 has 6 rows");

    auto r2 = taql_execute("SELECT FROM t WHERE FIELD_ID == 2", main);
    check(r2.rows.size() == 6, "TaQL: field 2 has 6 rows");

    auto r3 = taql_execute("COUNT FROM t WHERE OBSERVATION_ID == 1", main);
    check(r3.affected_rows == 12, "TaQL: obs 1 has 12 rows");

    // MSSelection evaluate
    MsSelection sel;
    sel.set_scan_expr("1,2");
    auto eval_r = sel.evaluate(ms);
    check(eval_r.rows.size() == 12, "MSSel: scan 1,2 -> 12 rows");

    // MSSelection to_taql_where + TaQL execution parity
    auto where = sel.to_taql_where(ms);
    auto taql_r = taql_execute("SELECT FROM t WHERE " + where, main);
    check(taql_r.rows.size() == eval_r.rows.size(),
          "MSSel -> TaQL WHERE parity (scan 1,2)");

    // Combined MSSelection
    MsSelection sel2;
    sel2.set_field_expr("0");
    sel2.set_observation_expr("1");
    auto eval_r2 = sel2.evaluate(ms);
    auto where2 = sel2.to_taql_where(ms);
    auto taql_r2 = taql_execute("SELECT FROM t WHERE " + where2, main);
    check(taql_r2.rows.size() == eval_r2.rows.size(),
          "MSSel -> TaQL WHERE parity (field 0 + obs 1)");

    // MSSelection with TaQL injection
    MsSelection sel3;
    sel3.set_scan_expr("3");
    sel3.set_taql_expr("ANTENNA1 == 0");
    auto eval_r3 = sel3.evaluate(ms);
    check(eval_r3.rows.size() == 3, "MSSel: scan 3 + ant1==0 -> 3 rows");

    // TaQL CALC with MS context
    auto cr = taql_execute("CALC 2.0 + 3.0", main);
    check(!cr.values.empty() && std::get<double>(cr.values[0]) == 5.0,
          "TaQL CALC: 2+3=5");

    // ROWNR in MS context
    auto rn = taql_execute("SELECT FROM t WHERE ROWNR() < 3", main);
    check(rn.rows.size() == 3, "TaQL ROWNR: rows 0,1,2");

    // SHOW command
    auto sh = taql_execute("SHOW", main);
    check(!sh.show_text.empty(), "TaQL SHOW returns text");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Test: API consistency — all MSSelection accessors return expected types
// ---------------------------------------------------------------------------

static void test_api_consistency() {
    auto path = fs::temp_directory_path() / "p11_integ_api";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_integration_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0,1");
    sel.set_field_expr("0");
    sel.set_scan_expr("1");
    sel.set_observation_expr("0");

    auto result = sel.evaluate(ms);

    // Result vectors populated
    check(!result.rows.empty(), "API: rows non-empty");
    check(!result.antennas.empty(), "API: antennas non-empty");
    check(!result.fields.empty(), "API: fields non-empty");
    check(!result.scans.empty(), "API: scans non-empty");
    check(!result.observations.empty(), "API: observations non-empty");

    // Expression accessors round-trip
    check(sel.antenna_expr().has_value(), "API: antenna_expr has value");
    check(*sel.antenna_expr() == "0,1", "API: antenna_expr correct");
    check(sel.field_expr().has_value(), "API: field_expr has value");
    check(sel.scan_expr().has_value(), "API: scan_expr has value");
    check(sel.observation_expr().has_value(), "API: observation_expr has value");

    // has_selection and clear
    check(sel.has_selection(), "API: has_selection true");
    sel.clear();
    check(!sel.has_selection(), "API: has_selection false after clear");
    check(!sel.antenna_expr().has_value(), "API: antenna_expr empty after clear");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_full_pipeline();
    test_api_consistency();

    std::cout << "phase11_integration_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
