// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file ms_selection_parser_test.cpp
/// @brief Tests for W5 MS selection parser parity: observation, array, feed,
///        TaQL injection, scan bounds, clear/reset, combined expressions.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

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

/// Build test MS with varied observation_id, array_id, feed1, feed2 values.
/// 4 antennas, 2 fields, 1 SPW, 3 scans, 2 observations, 2 arrays.
/// 12 rows with systematic variation.
static MeasurementSet make_w5_test_ms(const fs::path& path) {
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

    writer.add_field({.name = "SRC0",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "SRC1",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    writer.add_spectral_window({.num_chan = 64,
                                .name = "SPW0",
                                .ref_frequency = 1.4e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});

    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

    // 2 observations
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "obs0",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "obs1",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});

    // 2 states
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE_TARGET.ON_SOURCE",
                      .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "CALIBRATE_BANDPASS.ON_SOURCE",
                      .flag_row = false});

    // 12 rows with varied obs_id, array_id, feed1, feed2
    struct Row {
        std::int32_t a1, a2, fld, dd, scan, state, obs, arr, f1, f2;
        double time;
    };
    Row rows[] = {
        //  a1 a2  fld dd scan state obs arr f1 f2  time
        {0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 4.8e9},       // row 0
        {0, 2, 0, 0, 1, 0, 0, 0, 0, 1, 4.8e9},       // row 1
        {1, 2, 0, 0, 1, 0, 0, 1, 0, 0, 4.8e9},       // row 2
        {0, 3, 0, 0, 2, 0, 0, 1, 1, 1, 4.8e9 + 10},  // row 3 (feed auto)
        {0, 1, 1, 0, 2, 1, 1, 0, 0, 1, 4.8e9 + 20},  // row 4
        {0, 2, 1, 0, 2, 1, 1, 0, 0, 0, 4.8e9 + 20},  // row 5
        {1, 3, 1, 0, 3, 1, 1, 1, 1, 0, 4.8e9 + 30},  // row 6
        {2, 3, 0, 0, 3, 0, 0, 0, 0, 0, 4.8e9 + 30},  // row 7
        {0, 1, 0, 0, 3, 0, 0, 1, 0, 0, 4.8e9 + 40},  // row 8
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 4.8e9},       // row 9 (auto-corr)
        {1, 1, 1, 0, 2, 1, 1, 0, 1, 1, 4.8e9 + 20},  // row 10 (auto-corr)
        {2, 3, 1, 0, 3, 1, 1, 1, 0, 1, 4.8e9 + 40},  // row 11
    };

    for (const auto& rd : rows) {
        writer.add_row({.antenna1 = rd.a1,
                        .antenna2 = rd.a2,
                        .array_id = rd.arr,
                        .data_desc_id = rd.dd,
                        .exposure = 10.0,
                        .feed1 = rd.f1,
                        .feed2 = rd.f2,
                        .field_id = rd.fld,
                        .flag_row = false,
                        .interval = 10.0,
                        .observation_id = rd.obs,
                        .processor_id = 0,
                        .scan_number = rd.scan,
                        .state_id = rd.state,
                        .time = rd.time,
                        .time_centroid = rd.time,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
    }
    writer.flush();
    return MeasurementSet::open(path);
}

// ---------------------------------------------------------------------------
// Observation selection
// ---------------------------------------------------------------------------

static void test_observation_id() {
    auto path = fs::temp_directory_path() / "mssel_w5_obs";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr("0");
    auto r = sel.evaluate(ms);
    // obs_id=0: rows 0,1,2,3,7,8,9
    check(r.rows.size() == 7, "obs=0 returns 7 rows");
    check(r.observations.size() == 1, "obs result has 1 ID");
    check(r.observations[0] == 0, "obs result ID is 0");

    sel.clear();
    sel.set_observation_expr("1");
    r = sel.evaluate(ms);
    // obs_id=1: rows 4,5,6,10,11
    check(r.rows.size() == 5, "obs=1 returns 5 rows");

    sel.clear();
    sel.set_observation_expr("0,1");
    r = sel.evaluate(ms);
    check(r.rows.size() == 12, "obs=0,1 returns all 12 rows");

    fs::remove_all(path);
}

static void test_observation_range() {
    auto path = fs::temp_directory_path() / "mssel_w5_obs_rng";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr("0~1");
    auto r = sel.evaluate(ms);
    check(r.rows.size() == 12, "obs=0~1 returns all 12 rows");

    fs::remove_all(path);
}

static void test_observation_bounds() {
    auto path = fs::temp_directory_path() / "mssel_w5_obs_bnd";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr(">0");
    auto r = sel.evaluate(ms);
    check(r.rows.size() == 5, "obs>0 returns 5 rows (obs_id=1 only)");

    sel.clear();
    sel.set_observation_expr("<1");
    r = sel.evaluate(ms);
    check(r.rows.size() == 7, "obs<1 returns 7 rows (obs_id=0 only)");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Array selection
// ---------------------------------------------------------------------------

static void test_array_id() {
    auto path = fs::temp_directory_path() / "mssel_w5_arr";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_array_expr("0");
    auto r = sel.evaluate(ms);
    // arr=0: rows 0,1,4,5,7,9,10
    check(r.rows.size() == 7, "arr=0 returns 7 rows");
    check(r.arrays.size() == 1, "arr result has 1 ID");

    sel.clear();
    sel.set_array_expr("1");
    r = sel.evaluate(ms);
    // arr=1: rows 2,3,6,8,11
    check(r.rows.size() == 5, "arr=1 returns 5 rows");

    fs::remove_all(path);
}

static void test_array_bounds() {
    auto path = fs::temp_directory_path() / "mssel_w5_arr_bnd";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_array_expr(">0");
    auto r = sel.evaluate(ms);
    check(r.rows.size() == 5, "arr>0 returns 5 rows");

    sel.clear();
    sel.set_array_expr("<1");
    r = sel.evaluate(ms);
    check(r.rows.size() == 7, "arr<1 returns 7 rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Feed selection
// ---------------------------------------------------------------------------

static void test_feed_id() {
    auto path = fs::temp_directory_path() / "mssel_w5_feed";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_feed_expr("1");
    auto r = sel.evaluate(ms);
    // feed1=1 or feed2=1: rows 1,3,4,6,10,11
    check(r.rows.size() == 6, "feed=1 returns 6 rows");
    check(!r.feeds.empty(), "feed result has IDs");

    fs::remove_all(path);
}

static void test_feed_pair_cross() {
    auto path = fs::temp_directory_path() / "mssel_w5_feed_cross";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_feed_expr("0&1");
    auto r = sel.evaluate(ms);
    // Cross: (f1=0,f2=1) or (f1=1,f2=0): rows 1,4,6,11
    check(r.rows.size() == 4, "feed 0&1 cross returns 4 rows");

    fs::remove_all(path);
}

static void test_feed_pair_with_auto() {
    auto path = fs::temp_directory_path() / "mssel_w5_feed_auto";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_feed_expr("0&&1");
    auto r = sel.evaluate(ms);
    // With auto: cross(0,1) + auto(0,0) + auto(1,1)
    // Cross (0,1): rows 1,4,6,11
    // Auto (0,0): rows 0,2,5,7,8,9
    // Auto (1,1): rows 3,10
    check(r.rows.size() == 12, "feed 0&&1 with auto returns all 12 rows");

    fs::remove_all(path);
}

static void test_feed_auto_only() {
    auto path = fs::temp_directory_path() / "mssel_w5_feed_selfonly";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_feed_expr("1&&&");
    auto r = sel.evaluate(ms);
    // Auto-only for feed 1: f1=1 AND f2=1: rows 3, 10
    check(r.rows.size() == 2, "feed 1&&& auto-only returns 2 rows");

    fs::remove_all(path);
}

static void test_feed_negate() {
    auto path = fs::temp_directory_path() / "mssel_w5_feed_neg";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_feed_expr("!1");
    auto r = sel.evaluate(ms);
    // NOT (feed1=1 or feed2=1): rows where neither feed is 1
    // Rows with feed=1: 1,3,4,6,10,11 -> NOT: 0,2,5,7,8,9 = 6 rows
    check(r.rows.size() == 6, "feed !1 returns 6 rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Scan bounds
// ---------------------------------------------------------------------------

static void test_scan_bounds() {
    auto path = fs::temp_directory_path() / "mssel_w5_scan_bnd";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr(">2");
    auto r = sel.evaluate(ms);
    // scan>2: scan=3: rows 6,7,8,11
    check(r.rows.size() == 4, "scan>2 returns 4 rows");

    sel.clear();
    sel.set_scan_expr("<2");
    r = sel.evaluate(ms);
    // scan<2: scan=1: rows 0,1,2,9
    check(r.rows.size() == 4, "scan<2 returns 4 rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// TaQL injection
// ---------------------------------------------------------------------------

static void test_taql_injection() {
    auto path = fs::temp_directory_path() / "mssel_w5_taql";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_taql_expr("SCAN_NUMBER > 2");
    auto r = sel.evaluate(ms);
    // SCAN_NUMBER > 2 means scan=3: rows 6,7,8,11
    check(r.rows.size() == 4, "TaQL SCAN_NUMBER>2 returns 4 rows");

    fs::remove_all(path);
}

static void test_taql_combined() {
    auto path = fs::temp_directory_path() / "mssel_w5_taql_comb";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_taql_expr("SCAN_NUMBER > 1");
    sel.set_observation_expr("1");
    auto r = sel.evaluate(ms);
    // TaQL: scan>1 = scan 2,3: rows 3,4,5,6,7,8,10,11
    // obs=1: rows 4,5,6,10,11
    // AND: rows 4,5,6,10,11
    check(r.rows.size() == 5, "TaQL+obs combined returns 5 rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Clear and reset
// ---------------------------------------------------------------------------

static void test_clear_reset() {
    auto path = fs::temp_directory_path() / "mssel_w5_clear";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0");
    sel.set_scan_expr("1");
    check(sel.has_selection(), "has_selection before clear");

    sel.clear();
    check(!sel.has_selection(), "no selection after clear");

    auto r = sel.evaluate(ms);
    check(r.rows.size() == 12, "no selection returns all 12 rows");

    sel.set_observation_expr("0");
    sel.reset();
    check(!sel.has_selection(), "no selection after reset");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Combined new categories
// ---------------------------------------------------------------------------

static void test_combined_new_categories() {
    auto path = fs::temp_directory_path() / "mssel_w5_combined";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_w5_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr("0");
    sel.set_array_expr("1");
    auto r = sel.evaluate(ms);
    // obs=0 AND arr=1: rows where obs_id=0 AND array_id=1
    // obs=0: 0,1,2,3,7,8,9
    // arr=1: 2,3,6,8,11
    // AND: 2,3,8
    check(r.rows.size() == 3, "obs=0 AND arr=1 returns 3 rows");

    fs::remove_all(path);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    test_observation_id();
    test_observation_range();
    test_observation_bounds();
    test_array_id();
    test_array_bounds();
    test_feed_id();
    test_feed_pair_cross();
    test_feed_pair_with_auto();
    test_feed_auto_only();
    test_feed_negate();
    test_scan_bounds();
    test_taql_injection();
    test_taql_combined();
    test_clear_reset();
    test_combined_new_categories();

    std::cout << "ms_selection_parser_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
