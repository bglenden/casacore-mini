/// @file ms_selection_parity_extended_test.cpp
/// @brief P12-W8 tests for MSSelection parser/evaluator closure:
///   regex patterns, antenna &&/&&&, field negation/range/bounds,
///   SPW name/frequency/range, time strings, UV units, state bounds.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

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
           ("mssel_w8_" + suffix + "_" +
            std::to_string(std::hash<std::string>{}(std::to_string(getpid()))));
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) fs::remove_all(p);
}

/// Build test MS with known data for W8 tests.
/// 4 antennas (ANT0-ANT3), 2 fields (3C273, M31), 2 SPWs (SPW0 @ 1.4GHz, SPW1 @ 3.0GHz),
/// 2 states, 16 rows with all baselines including auto-correlations.
static MeasurementSet make_w8_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // 4 antennas with positions for baseline length tests
    writer.add_antenna({.name = "ANT0",
                        .station = "PAD0",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {0.0, 0.0, 0.0},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT1",
                        .station = "PAD1",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {100.0, 0.0, 0.0},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT2",
                        .station = "PAD2",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {0.0, 200.0, 0.0},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT3",
                        .station = "PAD3",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {300.0, 400.0, 0.0},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});

    // 2 fields
    writer.add_field({.name = "3C273",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "M31",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    // 2 SPWs
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
    writer.add_spectral_window({.num_chan = 128,
                                .name = "SPW1",
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

    // 2 data descriptions (1:1 with SPWs)
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

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

    // MJD for 2020/01/01 = 58849, seconds = 58849 * 86400 = 5084697600
    double time_base = 58849.0 * 86400.0;

    // 16 rows: 4 auto-correlations + 6 cross baselines + 6 more for second time/field
    // Using both auto (a1==a2) and cross baselines
    struct RowData {
        int a1, a2, dd, field, scan, state;
        double time;
        std::vector<double> uvw;
    };
    // clang-format off
    std::vector<RowData> rows = {
        // Auto-correlations at time0, field 0, scan 1
        {0, 0, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        {1, 1, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        {2, 2, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        {3, 3, 0, 0, 1, 0, time_base,       {0.0, 0.0, 0.0}},
        // Cross baselines at time0, field 0, scan 1
        {0, 1, 0, 0, 1, 0, time_base,       {100.0, 0.0, 0.0}},
        {0, 2, 0, 0, 1, 0, time_base,       {0.0, 200.0, 0.0}},
        {0, 3, 0, 0, 1, 0, time_base,       {300.0, 400.0, 0.0}},
        {1, 2, 0, 0, 1, 0, time_base,       {-100.0, 200.0, 0.0}},
        // Cross baselines at time1 (1 hour later), field 1, scan 2, SPW1
        {0, 1, 1, 1, 2, 1, time_base + 3600.0, {100.0, 0.0, 0.0}},
        {0, 2, 1, 1, 2, 1, time_base + 3600.0, {0.0, 200.0, 0.0}},
        {0, 3, 1, 1, 2, 1, time_base + 3600.0, {300.0, 400.0, 0.0}},
        {1, 3, 1, 1, 2, 1, time_base + 3600.0, {200.0, 400.0, 0.0}},
        // More rows at time2 (2 hours later), field 0, scan 3
        {2, 3, 0, 0, 3, 0, time_base + 7200.0, {300.0, 200.0, 0.0}},
        {1, 2, 0, 0, 3, 0, time_base + 7200.0, {-100.0, 200.0, 0.0}},
        // UV distance tests: large UV
        {0, 3, 0, 0, 1, 0, time_base,       {500.0, 500.0, 0.0}},
        // Small UV
        {0, 1, 0, 0, 1, 0, time_base,       {5.0, 5.0, 0.0}},
    };
    // clang-format on

    for (const auto& row : rows) {
        writer.add_row({.antenna1 = row.a1,
                        .antenna2 = row.a2,
                        .data_desc_id = row.dd,
                        .exposure = 10.0,
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
// Test: Antenna &&  (with-auto baseline)
// ============================================================================
static void test_antenna_with_auto() {
    auto path = make_temp_dir("ant_wa");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0&&1");
    auto result = sel.evaluate(ms);

    // Should include cross baseline 0-1 (rows 4, 8, 15) AND auto 0-0 (row 0) AND auto 1-1 (row 1)
    bool has_auto0 = false, has_auto1 = false, has_cross = false;
    for (auto r : result.rows) {
        // Check auto-correlations
        if (r == 0) has_auto0 = true;
        if (r == 1) has_auto1 = true;
        // Check cross baselines (rows with a1=0,a2=1 or vice versa)
        if (r == 4 || r == 8 || r == 15) has_cross = true;
    }
    check(has_auto0, "antenna && includes auto-corr 0-0");
    check(has_auto1, "antenna && includes auto-corr 1-1");
    check(has_cross, "antenna && includes cross baseline 0-1");

    cleanup(path);
}

// ============================================================================
// Test: Antenna &&&  (auto-only baseline)
// ============================================================================
static void test_antenna_auto_only() {
    auto path = make_temp_dir("ant_ao");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0&&&");
    auto result = sel.evaluate(ms);

    // Should only include row 0 (antenna1=0, antenna2=0)
    check(result.rows.size() == 1, "antenna &&& selects exactly 1 row");
    check(!result.rows.empty() && result.rows[0] == 0, "antenna &&& selects auto-corr 0-0");

    cleanup(path);
}

// ============================================================================
// Test: Antenna regex pattern
// ============================================================================
static void test_antenna_regex() {
    auto path = make_temp_dir("ant_rx");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("ANT[02]");
    auto result = sel.evaluate(ms);

    // Should match ANT0 (id=0) and ANT2 (id=2)
    check(result.antennas.size() == 2, "antenna regex selects 2 antennas");
    bool has0 = false, has2 = false;
    for (auto a : result.antennas) {
        if (a == 0) has0 = true;
        if (a == 2) has2 = true;
    }
    check(has0 && has2, "antenna regex matches ANT0 and ANT2");

    cleanup(path);
}

// ============================================================================
// Test: Field negation
// ============================================================================
static void test_field_negation() {
    auto path = make_temp_dir("fld_neg");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("!0");
    auto result = sel.evaluate(ms);

    // Field 0 rows: rows 0-8,12-15 (field_id=0); Field 1 rows: rows 8-11 (field_id=1)
    // "!0" should exclude field 0 rows, leaving only field 1 rows
    check(!result.rows.empty(), "field negation returns rows");
    // Verify no field-0 rows are selected
    // Field 1 rows are at indices 8,9,10,11
    check(result.rows.size() == 4, "field !0 returns 4 rows (field 1 only)");

    cleanup(path);
}

// ============================================================================
// Test: Field range
// ============================================================================
static void test_field_range() {
    auto path = make_temp_dir("fld_rng");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("0~1");
    auto result = sel.evaluate(ms);

    // Should include all rows (both field 0 and field 1)
    check(result.rows.size() == 16, "field range 0~1 selects all 16 rows");

    cleanup(path);
}

// ============================================================================
// Test: Field bounds
// ============================================================================
static void test_field_bounds() {
    auto path = make_temp_dir("fld_bnd");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_field_expr(">0");
    auto result = sel.evaluate(ms);

    // field_id > 0 → only field 1
    check(result.rows.size() == 4, "field >0 selects field 1 rows only");

    cleanup(path);
}

// ============================================================================
// Test: Field regex
// ============================================================================
static void test_field_regex() {
    auto path = make_temp_dir("fld_rx");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("3C.*");
    auto result = sel.evaluate(ms);

    // "3C.*" matches "3C273"
    check(result.fields.size() == 1 && result.fields[0] == 0, "field regex matches 3C273");

    cleanup(path);
}

// ============================================================================
// Test: SPW by name
// ============================================================================
static void test_spw_by_name() {
    auto path = make_temp_dir("spw_nm");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("SPW0");
    auto result = sel.evaluate(ms);

    check(result.spws.size() == 1 && result.spws[0] == 0, "SPW by name selects SPW0");
    // SPW0 maps to DD0; rows with dd=0
    check(!result.rows.empty(), "SPW by name returns rows");

    cleanup(path);
}

// ============================================================================
// Test: SPW range
// ============================================================================
static void test_spw_range() {
    auto path = make_temp_dir("spw_rng");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("0~1");
    auto result = sel.evaluate(ms);

    check(result.spws.size() == 2, "SPW range 0~1 selects 2 SPWs");
    check(result.rows.size() == 16, "SPW range 0~1 selects all rows");

    cleanup(path);
}

// ============================================================================
// Test: SPW by frequency
// ============================================================================
static void test_spw_by_frequency() {
    auto path = make_temp_dir("spw_frq");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    // Select SPW at 1.4 GHz
    MsSelection sel;
    sel.set_spw_expr("1.4GHz");
    auto result = sel.evaluate(ms);

    check(result.spws.size() == 1 && result.spws[0] == 0, "SPW by frequency matches 1.4GHz");

    cleanup(path);
}

// ============================================================================
// Test: SPW frequency range
// ============================================================================
static void test_spw_freq_range() {
    auto path = make_temp_dir("spw_frr");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("1.0~2.0GHz");
    auto result = sel.evaluate(ms);

    // Only SPW0 (1.4 GHz) is in range 1.0-2.0 GHz
    check(result.spws.size() == 1 && result.spws[0] == 0, "SPW freq range 1.0~2.0GHz matches SPW0");

    cleanup(path);
}

// ============================================================================
// Test: SPW glob pattern
// ============================================================================
static void test_spw_glob() {
    auto path = make_temp_dir("spw_glb");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("SPW*");
    auto result = sel.evaluate(ms);

    check(result.spws.size() == 2, "SPW glob SPW* matches both SPWs");

    cleanup(path);
}

// ============================================================================
// Test: Channel stride
// ============================================================================
static void test_channel_stride() {
    auto path = make_temp_dir("ch_str");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("0:0~63^2");
    auto result = sel.evaluate(ms);

    check(result.spws.size() == 1 && result.spws[0] == 0, "channel stride selects SPW0");
    // Channel ranges should be stored: {spw=0, start=0, end=63}
    check(result.channel_ranges.size() == 3, "channel stride stores range triple");
    check(result.channel_ranges[0] == 0, "channel stride spw_id=0");
    check(result.channel_ranges[1] == 0, "channel stride start=0");
    check(result.channel_ranges[2] == 63, "channel stride end=63");

    cleanup(path);
}

// ============================================================================
// Test: Time string selection
// ============================================================================
static void test_time_string() {
    auto path = make_temp_dir("time_str");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    // MJD for 2020/01/01 = 58849, seconds = 58849 * 86400 = 5084697600
    // Time base in our test MS is time_base = 58849 * 86400

    // Select time > 2020/01/01/00:30:00 (30 min = 1800 seconds after midnight)
    MsSelection sel;
    sel.set_time_expr(">2020/01/01/00:30:00");
    auto result = sel.evaluate(ms);

    // time_base = 5084697600. Rows at time_base are NOT > time_base+1800.
    // Only rows at time_base+3600 (1 hour) and time_base+7200 (2 hours) qualify.
    // Those are rows 8-13 (6 rows)
    check(result.rows.size() == 6, "time string >2020/01/01/00:30:00 selects 6 rows");

    cleanup(path);
}

// ============================================================================
// Test: Time string range
// ============================================================================
static void test_time_string_range() {
    auto path = make_temp_dir("time_rng");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    // Select time range from 2019/12/31/23:00:00 to 2020/01/01/00:30:00
    MsSelection sel;
    sel.set_time_expr("2019/12/31/23:00:00~2020/01/01/00:30:00");
    auto result = sel.evaluate(ms);

    // Only rows at time_base (= 2020/01/01/00:00:00) qualify
    // time_base = 5084697600 seconds
    // 2019/12/31/23:00:00 = time_base - 3600
    // 2020/01/01/00:30:00 = time_base + 1800
    // Rows at time_base: 0-7, 14, 15 = 10 rows
    check(result.rows.size() == 10, "time string range selects time_base rows");

    cleanup(path);
}

// ============================================================================
// Test: UV distance with km unit
// ============================================================================
static void test_uvdist_km() {
    auto path = make_temp_dir("uv_km");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    // Select UV distance > 0.5 km = 500 m
    MsSelection sel;
    sel.set_uvdist_expr(">0.5km");
    auto result = sel.evaluate(ms);

    // Row 14 has uvw = {500, 500, 0}, dist = sqrt(500000) ≈ 707 > 500
    // Row 6, 10 have uvw = {300, 400, 0}, dist = 500 — NOT > 500 (strictly greater)
    // So only row 14 qualifies
    check(!result.rows.empty(), "UV >0.5km returns rows");
    bool has_row14 = false;
    for (auto r : result.rows) {
        if (r == 14) has_row14 = true;
    }
    check(has_row14, "UV >0.5km includes large-UV row");

    cleanup(path);
}

// ============================================================================
// Test: UV distance with m unit
// ============================================================================
static void test_uvdist_m() {
    auto path = make_temp_dir("uv_m");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_uvdist_expr("<10m");
    auto result = sel.evaluate(ms);

    // Rows with uvdist < 10: auto-corrs (0,0,0) → 0 m → rows 0-3, and row 15 (5,5,0) → ~7.07 m
    check(result.rows.size() == 5, "UV <10m selects auto-corrs + small UV row");

    cleanup(path);
}

// ============================================================================
// Test: State bounds
// ============================================================================
static void test_state_bounds() {
    auto path = make_temp_dir("st_bnd");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_state_expr(">0");
    auto result = sel.evaluate(ms);

    // State > 0 → only state_id=1 rows (rows 8-11)
    check(result.rows.size() == 4, "state >0 selects 4 rows");

    cleanup(path);
}

// ============================================================================
// Test: State bounds <
// ============================================================================
static void test_state_bounds_lt() {
    auto path = make_temp_dir("st_blt");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_state_expr("<1");
    auto result = sel.evaluate(ms);

    // State < 1 → only state_id=0 rows (rows 0-7, 12-15)
    check(result.rows.size() == 12, "state <1 selects 12 rows");

    cleanup(path);
}

// ============================================================================
// Test: State regex pattern
// ============================================================================
static void test_state_regex() {
    auto path = make_temp_dir("st_rx");
    cleanup(path);
    auto ms = make_w8_test_ms(path);

    MsSelection sel;
    sel.set_state_expr("CALIBRATE.*");
    auto result = sel.evaluate(ms);

    // "CALIBRATE.*" matches "CALIBRATE_BANDPASS.ON_SOURCE" (state 1)
    check(result.states.size() == 1 && result.states[0] == 1, "state regex matches CALIBRATE");
    check(result.rows.size() == 4, "state regex selects 4 rows");

    cleanup(path);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    run(test_antenna_with_auto, "antenna_with_auto");
    run(test_antenna_auto_only, "antenna_auto_only");
    run(test_antenna_regex, "antenna_regex");
    run(test_field_negation, "field_negation");
    run(test_field_range, "field_range");
    run(test_field_bounds, "field_bounds");
    run(test_field_regex, "field_regex");
    run(test_spw_by_name, "spw_by_name");
    run(test_spw_range, "spw_range");
    run(test_spw_by_frequency, "spw_by_frequency");
    run(test_spw_freq_range, "spw_freq_range");
    run(test_spw_glob, "spw_glob");
    run(test_channel_stride, "channel_stride");
    run(test_time_string, "time_string");
    run(test_time_string_range, "time_string_range");
    run(test_uvdist_km, "uvdist_km");
    run(test_uvdist_m, "uvdist_m");
    run(test_state_bounds, "state_bounds");
    run(test_state_bounds_lt, "state_bounds_lt");
    run(test_state_regex, "state_regex");

    std::cout << "ms_selection_parity_extended_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? 1 : 0;
}
