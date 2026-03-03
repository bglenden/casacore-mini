// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file ms_selection_api_parity_test.cpp
/// @brief P12-W9 tests for MSSelection API/accessor/mode/error closure:
///   split antenna/feed lists, time/UV range outputs, DDID/pol maps,
///   error-collection mode, ParseLate semantics.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

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

static void run(void (*fn)(), const char* name) {
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION in " << name << ": " << e.what() << "\n";
    }
}

static fs::path make_temp_dir(const std::string& suffix) {
    return fs::temp_directory_path() /
           ("mssel_w9_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p))
        fs::remove_all(p);
}

/// Build test MS with known data for W9 tests.
/// 4 antennas, 2 fields, 2 SPWs (2 DDIDs), 2 polarizations, 2 states, feed pairs.
static MeasurementSet make_w9_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // 4 antennas
    for (int i = 0; i < 4; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {static_cast<double>(i * 100), 0.0, 0.0},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
    }

    // 2 fields
    writer.add_field({.name = "SRC_A",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "SRC_B",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    // 2 SPWs
    writer.add_spectral_window({.num_chan = 64,
                                .name = "SPW_L",
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
    writer.add_spectral_window({.num_chan = 128,
                                .name = "SPW_S",
                                .ref_frequency = 3.0e9,
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

    // 2 data descriptions: DDID 0 -> SPW 0, pol 0; DDID 1 -> SPW 1, pol 1
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 1, .flag_row = false});

    // 2 polarizations
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_polarization({.num_corr = 4, .corr_type = {5, 6, 7, 8}, .flag_row = false});

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

    // Observation
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "test",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});

    double time_base = 58849.0 * 86400.0;

    // 12 rows with varied baselines, feeds, times, UV
    // clang-format off
    struct RowData { int a1, a2, f1, f2, dd, field, scan, state; double time; std::vector<double> uvw; };
    std::vector<RowData> rows = {
        // Auto-correlations
        {0, 0, 0, 0, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        {1, 1, 1, 1, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        // Cross baselines with different feeds
        {0, 1, 0, 1, 0, 0, 1, 0, time_base,       {100.0, 0.0, 0.0}},
        {0, 2, 0, 0, 0, 0, 1, 0, time_base,       {0.0, 200.0, 0.0}},
        {1, 2, 1, 0, 0, 0, 1, 0, time_base,       {-100.0, 200.0, 0.0}},
        // Second time, field 1, SPW1
        {0, 1, 0, 1, 1, 1, 2, 1, time_base + 3600.0, {150.0, 0.0, 0.0}},
        {0, 3, 0, 0, 1, 1, 2, 1, time_base + 3600.0, {300.0, 400.0, 0.0}},
        {2, 3, 0, 0, 1, 1, 2, 1, time_base + 3600.0, {300.0, 200.0, 0.0}},
        // Third time, field 0, scan 3
        {0, 1, 0, 1, 0, 0, 3, 0, time_base + 7200.0, {100.0, 50.0, 0.0}},
        {1, 3, 1, 0, 0, 0, 3, 0, time_base + 7200.0, {200.0, 400.0, 0.0}},
        // Feed pair 1&1 (auto-feed)
        {0, 1, 1, 1, 0, 0, 1, 0, time_base,       {100.0, 0.0, 0.0}},
        // Feed pair 0&0 (auto-feed)
        {2, 3, 0, 0, 0, 0, 1, 0, time_base,       {300.0, 200.0, 0.0}},
    };
    // clang-format on

    for (const auto& row : rows) {
        writer.add_row({.antenna1 = row.a1,
                        .antenna2 = row.a2,
                        .data_desc_id = row.dd,
                        .exposure = 10.0,
                        .feed1 = row.f1,
                        .feed2 = row.f2,
                        .field_id = row.field,
                        .interval = 10.0,
                        .scan_number = row.scan,
                        .state_id = row.state,
                        .time = row.time,
                        .time_centroid = row.time,
                        .uvw = row.uvw,
                        .sigma = {},
                        .weight = {},
                        .data = {},
                        .flag = {}});
    }

    writer.flush();
    return MeasurementSet::open(path);
}

// ============================================================================
// Test: Antenna baseline pair populates antenna1_list / antenna2_list
// ============================================================================
static void test_antenna_baseline_lists() {
    auto path = make_temp_dir("ant_list");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        // Cross-only baseline "0&1"
        MsSelection sel;
        sel.set_antenna_expr("0&1");
        auto res = sel.evaluate(ms);
        check(!res.antenna1_list.empty(), "antenna1_list populated for baseline");
        check(res.antenna1_list[0] == 0, "antenna1_list[0] == 0");
        check(!res.antenna2_list.empty(), "antenna2_list populated for baseline");
        check(res.antenna2_list[0] == 1, "antenna2_list[0] == 1");
        std::cout << "  antenna baseline pair lists... PASS\n";
    }

    {
        // Auto-only "0&&&"
        MsSelection sel;
        sel.set_antenna_expr("0&&&");
        auto res = sel.evaluate(ms);
        check(res.antenna1_list[0] == 0, "auto-only antenna1_list[0] == 0");
        check(res.antenna2_list[0] == 0, "auto-only antenna2_list[0] == 0");
        std::cout << "  antenna auto-only lists... PASS\n";
    }

    {
        // ID list "0,1" -> both sides mirror
        MsSelection sel;
        sel.set_antenna_expr("0,1");
        auto res = sel.evaluate(ms);
        check(res.antenna1_list.size() == 2, "id-list antenna1_list size 2");
        check(res.antenna2_list.size() == 2, "id-list antenna2_list size 2");
        std::cout << "  antenna id-list lists... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: Feed pair populates feed1_list / feed2_list
// ============================================================================
static void test_feed_pair_lists() {
    auto path = make_temp_dir("feed_list");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        // Feed pair "0&1"
        MsSelection sel;
        sel.set_feed_expr("0&1");
        auto res = sel.evaluate(ms);
        check(!res.feed1_list.empty(), "feed1_list populated for pair");
        check(res.feed1_list[0] == 0, "feed1_list[0] == 0");
        check(!res.feed2_list.empty(), "feed2_list populated for pair");
        check(res.feed2_list[0] == 1, "feed2_list[0] == 1");
        std::cout << "  feed pair lists... PASS\n";
    }

    {
        // Feed auto-only "1&&&"
        MsSelection sel;
        sel.set_feed_expr("1&&&");
        auto res = sel.evaluate(ms);
        check(res.feed1_list[0] == 1, "auto-only feed1_list[0] == 1");
        check(res.feed2_list[0] == 1, "auto-only feed2_list[0] == 1");
        std::cout << "  feed auto-only lists... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: Time ranges populated after time selection
// ============================================================================
static void test_time_ranges() {
    auto path = make_temp_dir("time_rng");
    cleanup(path);
    auto ms = make_w9_test_ms(path);
    double time_base = 58849.0 * 86400.0;

    {
        // Range expression
        MsSelection sel;
        auto lo_str = std::to_string(time_base);
        auto hi_str = std::to_string(time_base + 3600.0);
        sel.set_time_expr(lo_str + "~" + hi_str);
        auto res = sel.evaluate(ms);
        check(res.time_ranges.size() == 1, "time_ranges has one entry");
        check(std::abs(res.time_ranges[0].lo - time_base) < 1.0, "time_ranges lo correct");
        check(std::abs(res.time_ranges[0].hi - (time_base + 3600.0)) < 1.0,
              "time_ranges hi correct");
        check(res.time_ranges[0].is_seconds, "time_ranges is_seconds for numeric");
        std::cout << "  time ranges (numeric)... PASS\n";
    }

    {
        // Bound expression > with date string
        MsSelection sel;
        sel.set_time_expr(">2020/01/01");
        auto res = sel.evaluate(ms);
        check(res.time_ranges.size() == 1, "time_ranges has entry for date bound");
        check(!res.time_ranges[0].is_seconds, "time_ranges !is_seconds for date string");
        // All 12 rows are at MJD 58849+, which is 2020/01/01, so >(midnight) should select later
        // rows
        std::cout << "  time ranges (date bound)... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: UV ranges populated after UV selection
// ============================================================================
static void test_uv_ranges() {
    auto path = make_temp_dir("uv_rng");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        MsSelection sel;
        sel.set_uvdist_expr(">100.0");
        auto res = sel.evaluate(ms);
        check(res.uv_ranges.size() == 1, "uv_ranges has one entry");
        check(std::abs(res.uv_ranges[0].lo - 100.0) < 0.01, "uv_ranges lo correct");
        check(res.uv_ranges[0].unit == UvUnit::Meters, "uv_ranges unit is Meters");
        std::cout << "  uv ranges... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: DDID / SPW / pol map from SPW selection
// ============================================================================
static void test_ddid_maps() {
    auto path = make_temp_dir("ddid_map");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        // Select SPW 0 -> DDID 0
        MsSelection sel;
        sel.set_spw_expr("0");
        auto res = sel.evaluate(ms);

        check(!res.data_desc_ids.empty(), "data_desc_ids populated");
        check(res.data_desc_ids[0] == 0, "data_desc_ids[0] == 0");
        check(res.ddid_to_spw.count(0) == 1, "ddid_to_spw has entry for DDID 0");
        check(res.ddid_to_spw.at(0) == 0, "ddid_to_spw[0] == SPW 0");
        check(res.ddid_to_pol_id.count(0) == 1, "ddid_to_pol_id has entry for DDID 0");
        check(res.ddid_to_pol_id.at(0) == 0, "ddid_to_pol_id[0] == pol 0");
        std::cout << "  DDID maps (SPW 0)... PASS\n";
    }

    {
        // Select SPW 1 -> DDID 1
        MsSelection sel;
        sel.set_spw_expr("1");
        auto res = sel.evaluate(ms);

        check(!res.data_desc_ids.empty(), "data_desc_ids populated for SPW 1");
        check(res.data_desc_ids[0] == 1, "data_desc_ids[0] == 1");
        check(res.ddid_to_spw.at(1) == 1, "ddid_to_spw[1] == SPW 1");
        check(res.ddid_to_pol_id.at(1) == 1, "ddid_to_pol_id[1] == pol 1");
        std::cout << "  DDID maps (SPW 1)... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: ErrorMode::CollectErrors — malformed expression doesn't throw
// ============================================================================
static void test_error_collection() {
    auto path = make_temp_dir("err_coll");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        MsSelection sel;
        sel.set_error_mode(ErrorMode::CollectErrors);
        sel.set_antenna_expr("INVALID_ANTENNA_NAME_XYZ");
        // Should NOT throw
        auto res = sel.evaluate(ms);
        check(!res.errors.empty(), "errors populated for invalid antenna");
        std::cout << "  error collection (antenna)... PASS\n";
    }

    {
        MsSelection sel;
        sel.set_error_mode(ErrorMode::CollectErrors);
        sel.set_field_expr("NO_SUCH_FIELD_ABCDEF");
        auto res = sel.evaluate(ms);
        check(!res.errors.empty(), "errors populated for invalid field");
        std::cout << "  error collection (field)... PASS\n";
    }

    {
        // Multiple bad expressions — errors accumulate
        MsSelection sel;
        sel.set_error_mode(ErrorMode::CollectErrors);
        sel.set_antenna_expr("BAD_ANT_999");
        sel.set_field_expr("BAD_FLD_999");
        auto res = sel.evaluate(ms);
        check(res.errors.size() >= 2, "multiple errors accumulated");
        std::cout << "  error collection (multiple)... PASS\n";
    }

    {
        // ThrowOnError mode — should throw
        MsSelection sel;
        sel.set_error_mode(ErrorMode::ThrowOnError);
        sel.set_antenna_expr("INVALID_NAME");
        bool threw = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "ThrowOnError throws on bad expression");
        std::cout << "  ThrowOnError mode... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: ParseMode::ParseLate — deferred parsing
// ============================================================================
static void test_parse_late() {
    auto path = make_temp_dir("parse_late");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        // ParseLate: setting an expression doesn't throw, evaluate does
        MsSelection sel;
        sel.set_parse_mode(ParseMode::ParseLate);
        sel.set_antenna_expr("0,1"); // valid — should work
        auto res = sel.evaluate(ms);
        check(!res.rows.empty(), "ParseLate valid expression works");
        check(res.antennas.size() == 2, "ParseLate antennas populated");
        std::cout << "  ParseLate (valid)... PASS\n";
    }

    {
        // ParseLate + CollectErrors
        MsSelection sel;
        sel.set_parse_mode(ParseMode::ParseLate);
        sel.set_error_mode(ErrorMode::CollectErrors);
        sel.set_scan_expr("1");
        sel.set_field_expr("NONEXISTENT_FIELD");
        auto res = sel.evaluate(ms);
        // Scan should work, field should produce an error
        check(!res.errors.empty(), "ParseLate collects errors");
        check(!res.scans.empty(), "ParseLate valid scan still works");
        std::cout << "  ParseLate + CollectErrors... PASS\n";
    }

    cleanup(path);
}

// ============================================================================
// Test: Mode accessors and clear/reset
// ============================================================================
static void test_mode_accessors() {
    {
        MsSelection sel;
        check(sel.parse_mode() == ParseMode::ParseNow, "default parse_mode is ParseNow");
        check(sel.error_mode() == ErrorMode::ThrowOnError, "default error_mode is ThrowOnError");

        sel.set_parse_mode(ParseMode::ParseLate);
        sel.set_error_mode(ErrorMode::CollectErrors);
        check(sel.parse_mode() == ParseMode::ParseLate, "parse_mode setter works");
        check(sel.error_mode() == ErrorMode::CollectErrors, "error_mode setter works");

        sel.clear();
        check(sel.parse_mode() == ParseMode::ParseNow, "clear resets parse_mode");
        check(sel.error_mode() == ErrorMode::ThrowOnError, "clear resets error_mode");
        std::cout << "  mode accessors and clear... PASS\n";
    }

    {
        MsSelection sel;
        sel.set_parse_mode(ParseMode::ParseLate);
        sel.set_error_mode(ErrorMode::CollectErrors);
        sel.set_antenna_expr("0");
        sel.reset();
        check(sel.parse_mode() == ParseMode::ParseNow, "reset resets parse_mode");
        check(sel.error_mode() == ErrorMode::ThrowOnError, "reset resets error_mode");
        check(!sel.has_selection(), "reset clears selection");
        std::cout << "  mode reset... PASS\n";
    }
}

// ============================================================================
// Test: to_taql_where still works correctly
// ============================================================================
static void test_taql_where_bridge() {
    auto path = make_temp_dir("taql_br");
    cleanup(path);
    auto ms = make_w9_test_ms(path);

    {
        MsSelection sel;
        sel.set_scan_expr("1,2");
        auto where = sel.to_taql_where(ms);
        check(!where.empty(), "to_taql_where produces output");
        check(where.find("SCAN_NUMBER") != std::string::npos, "to_taql_where contains SCAN_NUMBER");
        std::cout << "  to_taql_where bridge... PASS\n";
    }

    cleanup(path);
}

// ============================================================================

int main() {
    run(test_antenna_baseline_lists, "antenna_baseline_lists");
    run(test_feed_pair_lists, "feed_pair_lists");
    run(test_time_ranges, "time_ranges");
    run(test_uv_ranges, "uv_ranges");
    run(test_ddid_maps, "ddid_maps");
    run(test_error_collection, "error_collection");
    run(test_parse_late, "parse_late");
    run(test_mode_accessors, "mode_accessors");
    run(test_taql_where_bridge, "taql_where_bridge");

    std::cout << "ms_selection_api_parity_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
