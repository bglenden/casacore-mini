#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_sel_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Build a small test MS with known data for selection tests.
/// 4 antennas (ANT0-ANT3), 2 fields (3C273, M31), 2 SPWs, 2 scans,
/// 2 states, multiple rows spanning different baselines/times/fields.
static MeasurementSet make_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // 4 antennas.
    writer.add_antenna({.name = "ANT0",
                        .station = "PAD0",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT1",
                        .station = "PAD1",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT2",
                        .station = "PAD2",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT3",
                        .station = "PAD3",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});

    // 2 fields.
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

    // 2 SPWs.
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

    // 2 data descriptions: DD0→SPW0, DD1→SPW1.
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0, .flag_row = false});

    // 1 polarization.
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false}); // RR, LL

    // 1 observation.
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "test",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});

    // 2 states.
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

    // 10 rows:
    // Row 0: ant0-ant1, field0, dd0, scan1, state0, time=4.8e9, uvw=(100,200,50)
    // Row 1: ant0-ant2, field0, dd0, scan1, state0, time=4.8e9, uvw=(300,400,100)
    // Row 2: ant1-ant2, field0, dd0, scan1, state0, time=4.8e9, uvw=(200,100,30)
    // Row 3: ant0-ant3, field0, dd1, scan1, state0, time=4.8e9, uvw=(500,600,150)
    // Row 4: ant0-ant1, field1, dd0, scan2, state1, time=4.8e9+100, uvw=(110,210,55)
    // Row 5: ant0-ant2, field1, dd0, scan2, state1, time=4.8e9+100, uvw=(310,410,105)
    // Row 6: ant1-ant3, field1, dd1, scan2, state1, time=4.8e9+100, uvw=(400,300,80)
    // Row 7: ant2-ant3, field0, dd0, scan1, state0, time=4.8e9+50, uvw=(50,30,10)
    // Row 8: ant0-ant1, field0, dd0, scan1, state0, time=4.8e9+50, uvw=(10,10,5)
    // Row 9: ant0-ant1, field1, dd1, scan2, state1, time=4.8e9+200, uvw=(800,900,200)
    struct RowDef {
        std::int32_t ant1, ant2, field_id, dd_id, scan, state_id;
        double time;
        double u, v, w;
    };
    RowDef rows[] = {
        {0, 1, 0, 0, 1, 0, 4.8e9, 100, 200, 50},
        {0, 2, 0, 0, 1, 0, 4.8e9, 300, 400, 100},
        {1, 2, 0, 0, 1, 0, 4.8e9, 200, 100, 30},
        {0, 3, 0, 1, 1, 0, 4.8e9, 500, 600, 150},
        {0, 1, 1, 0, 2, 1, 4.8e9 + 100, 110, 210, 55},
        {0, 2, 1, 0, 2, 1, 4.8e9 + 100, 310, 410, 105},
        {1, 3, 1, 1, 2, 1, 4.8e9 + 100, 400, 300, 80},
        {2, 3, 0, 0, 1, 0, 4.8e9 + 50, 50, 30, 10},
        {0, 1, 0, 0, 1, 0, 4.8e9 + 50, 10, 10, 5},
        {0, 1, 1, 1, 2, 1, 4.8e9 + 200, 800, 900, 200},
    };

    for (const auto& rd : rows) {
        writer.add_row({.antenna1 = rd.ant1,
                        .antenna2 = rd.ant2,
                        .array_id = 0,
                        .data_desc_id = rd.dd_id,
                        .exposure = 0.0,
                        .feed1 = 0,
                        .feed2 = 0,
                        .field_id = rd.field_id,
                        .flag_row = false,
                        .interval = 10.0,
                        .observation_id = 0,
                        .processor_id = 0,
                        .scan_number = rd.scan,
                        .state_id = rd.state_id,
                        .time = rd.time,
                        .time_centroid = rd.time,
                        .uvw = {rd.u, rd.v, rd.w},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
    }

    writer.flush();
    return MeasurementSet::open(path);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_no_selection_returns_all_rows() {
    std::cout << "  no selection returns all rows... ";

    auto path = make_temp_dir("nosel");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    assert(!sel.has_selection());
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 10);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_id_selection() {
    std::cout << "  antenna ID selection... ";

    auto path = make_temp_dir("antid");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Select antenna 3 → rows where ANTENNA1=3 or ANTENNA2=3: rows 3,6,7,9
    MsSelection sel;
    sel.set_antenna_expr("3");
    auto result = sel.evaluate(ms);
    // Rows with ant 3: row3 (0-3), row6 (1-3), row7 (2-3), row9 (0-1)? No.
    // Row 3: ant0-ant3 -> yes
    // Row 6: ant1-ant3 -> yes
    // Row 7: ant2-ant3 -> yes
    // Row 9: ant0-ant1 -> no
    assert(result.rows.size() == 3);
    assert(result.rows[0] == 3);
    assert(result.rows[1] == 6);
    assert(result.rows[2] == 7);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_range_selection() {
    std::cout << "  antenna range selection... ";

    auto path = make_temp_dir("antrng");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Select antennas 0~1 → rows where ANTENNA1 or ANTENNA2 in {0,1}.
    // All rows have at least one of 0 or 1 except row7 (2-3).
    MsSelection sel;
    sel.set_antenna_expr("0~1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 9);
    // Check row7 (ant2-ant3) is excluded.
    for (auto r : result.rows) {
        assert(r != 7);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_baseline_selection() {
    std::cout << "  antenna baseline selection... ";

    auto path = make_temp_dir("antbl");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Baseline 0&1: rows where (ANTENNA1=0,ANTENNA2=1) or (ANTENNA1=1,ANTENNA2=0).
    // Rows: 0 (0-1), 4 (0-1), 8 (0-1), 9 (0-1) = 4 rows
    MsSelection sel;
    sel.set_antenna_expr("0&1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 4);
    assert(result.rows[0] == 0);
    assert(result.rows[1] == 4);
    assert(result.rows[2] == 8);
    assert(result.rows[3] == 9);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_wildcard_baseline() {
    std::cout << "  antenna wildcard baseline... ";

    auto path = make_temp_dir("antwild");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Baseline 3&*: all rows with ant3.
    MsSelection sel;
    sel.set_antenna_expr("3&*");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 3);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_negation() {
    std::cout << "  antenna negation... ";

    auto path = make_temp_dir("antneg");
    cleanup(path);
    auto ms = make_test_ms(path);

    // !3: exclude rows with antenna 3.
    // Rows with ant 3: 3, 6, 7 → 7 rows remain.
    MsSelection sel;
    sel.set_antenna_expr("!3");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 7);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_by_name() {
    std::cout << "  antenna by name... ";

    auto path = make_temp_dir("antname");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "ANT3" → same as "3".
    MsSelection sel;
    sel.set_antenna_expr("ANT3");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 3);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_field_id_selection() {
    std::cout << "  field ID selection... ";

    auto path = make_temp_dir("fldid");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Field 1 (M31): rows 4, 5, 6, 9.
    MsSelection sel;
    sel.set_field_expr("1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 4);
    assert(result.rows[0] == 4);
    assert(result.rows[1] == 5);
    assert(result.rows[2] == 6);
    assert(result.rows[3] == 9);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_field_by_name() {
    std::cout << "  field by name... ";

    auto path = make_temp_dir("fldname");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("3C273");
    auto result = sel.evaluate(ms);
    // Field 0 rows: 0,1,2,3,7,8 → 6 rows.
    assert(result.rows.size() == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_field_glob_pattern() {
    std::cout << "  field glob pattern... ";

    auto path = make_temp_dir("fldglob");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "3C*" matches "3C273".
    MsSelection sel;
    sel.set_field_expr("3C*");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 6);

    // "M*" matches "M31".
    MsSelection sel2;
    sel2.set_field_expr("M*");
    auto result2 = sel2.evaluate(ms);
    assert(result2.rows.size() == 4);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_selection() {
    std::cout << "  SPW selection... ";

    auto path = make_temp_dir("spw");
    cleanup(path);
    auto ms = make_test_ms(path);

    // SPW 0 → DD0 → rows with data_desc_id=0: 0,1,2,4,5,7,8.
    MsSelection sel;
    sel.set_spw_expr("0");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 7);

    // SPW 1 → DD1 → rows with data_desc_id=1: 3,6,9.
    MsSelection sel2;
    sel2.set_spw_expr("1");
    auto result2 = sel2.evaluate(ms);
    assert(result2.rows.size() == 3);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_with_channels() {
    std::cout << "  SPW with channel ranges... ";

    auto path = make_temp_dir("spwch");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "0:0~63" — selects SPW 0, channels 0-63.
    // Rows are same as SPW 0.
    MsSelection sel;
    sel.set_spw_expr("0:0~63");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 7);
    // Channel ranges stored: {spw=0, start=0, end=63}.
    assert(result.channel_ranges.size() == 3);
    assert(result.channel_ranges[0] == 0);
    assert(result.channel_ranges[1] == 0);
    assert(result.channel_ranges[2] == 63);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_all_wildcard() {
    std::cout << "  SPW wildcard '*'... ";

    auto path = make_temp_dir("spwall");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("*");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 10);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_scan_selection() {
    std::cout << "  scan selection... ";

    auto path = make_temp_dir("scan");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Scan 1: rows 0,1,2,3,7,8 → 6 rows.
    MsSelection sel;
    sel.set_scan_expr("1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 6);

    // Scan 2: rows 4,5,6,9 → 4 rows.
    MsSelection sel2;
    sel2.set_scan_expr("2");
    auto result2 = sel2.evaluate(ms);
    assert(result2.rows.size() == 4);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_scan_range() {
    std::cout << "  scan range... ";

    auto path = make_temp_dir("scanrng");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr("1~2");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 10);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_scan_negation() {
    std::cout << "  scan negation... ";

    auto path = make_temp_dir("scanneg");
    cleanup(path);
    auto ms = make_test_ms(path);

    // !1: exclude scan 1 → rows with scan 2: 4,5,6,9.
    MsSelection sel;
    sel.set_scan_expr("!1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 4);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_time_greater_than() {
    std::cout << "  time > selection... ";

    auto path = make_temp_dir("timegt");
    cleanup(path);
    auto ms = make_test_ms(path);

    // time > 4.8e9+50: rows with time > 4800000050.
    // Row times: 4.8e9 (rows 0,1,2,3), 4.8e9+50 (rows 7,8), 4.8e9+100 (rows 4,5,6), 4.8e9+200 (row
    // 9). Strictly > 4.8e9+50: rows 4,5,6,9 → 4 rows.
    MsSelection sel;
    sel.set_time_expr(">" + std::to_string(4.8e9 + 50.0));
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 4);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_time_range() {
    std::cout << "  time range selection... ";

    auto path = make_temp_dir("timerng");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Range: 4.8e9+25 ~ 4.8e9+150.
    // Matches: 4.8e9+50 (rows 7,8), 4.8e9+100 (rows 4,5,6) → 5 rows.
    double lo = 4.8e9 + 25.0;
    double hi = 4.8e9 + 150.0;
    MsSelection sel;
    sel.set_time_expr(std::to_string(lo) + "~" + std::to_string(hi));
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 5);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_uvdist_selection() {
    std::cout << "  UV-distance selection... ";

    auto path = make_temp_dir("uvdist");
    cleanup(path);
    auto ms = make_test_ms(path);

    // UVdist > 500: sqrt(u^2+v^2) > 500.
    // Row 0: sqrt(100^2+200^2) = 223 → no
    // Row 1: sqrt(300^2+400^2) = 500 → no (not strictly >)
    // Row 3: sqrt(500^2+600^2) = 781 → yes
    // Row 5: sqrt(310^2+410^2) = 514 → yes
    // Row 6: sqrt(400^2+300^2) = 500 → no
    // Row 9: sqrt(800^2+900^2) = 1204 → yes
    MsSelection sel;
    sel.set_uvdist_expr(">500");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 3);
    assert(result.rows[0] == 3);
    assert(result.rows[1] == 5);
    assert(result.rows[2] == 9);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_uvdist_range() {
    std::cout << "  UV-distance range... ";

    auto path = make_temp_dir("uvdrng");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Range 100~300.
    // Row 0: 223 → yes
    // Row 2: sqrt(200^2+100^2) = 223 → yes
    // Row 4: sqrt(110^2+210^2) = 237 → yes
    // Row 7: sqrt(50^2+30^2) = 58 → no
    // Row 8: sqrt(10^2+10^2) = 14 → no
    MsSelection sel;
    sel.set_uvdist_expr("100~300");
    auto result = sel.evaluate(ms);
    // Need to count carefully:
    // Row 0: 223 → yes
    // Row 1: 500 → no
    // Row 2: 223 → yes
    // Row 3: 781 → no
    // Row 4: 237 → yes
    // Row 5: 514 → no
    // Row 6: 500 → no
    // Row 7: 58 → no
    // Row 8: 14 → no
    // Row 9: 1204 → no
    assert(result.rows.size() == 3);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_corr_selection() {
    std::cout << "  correlation selection... ";

    auto path = make_temp_dir("corr");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_corr_expr("RR,LL");
    auto result = sel.evaluate(ms);
    // Correlation selection doesn't filter rows.
    assert(result.rows.size() == 10);
    assert(result.correlations.size() == 2);
    assert(result.correlations[0] == "RR");
    assert(result.correlations[1] == "LL");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_state_id_selection() {
    std::cout << "  state ID selection... ";

    auto path = make_temp_dir("stateid");
    cleanup(path);
    auto ms = make_test_ms(path);

    // State 0: rows 0,1,2,3,7,8 → 6 rows.
    MsSelection sel;
    sel.set_state_expr("0");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_state_pattern_selection() {
    std::cout << "  state pattern selection... ";

    auto path = make_temp_dir("statepattern");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "*CALIBRATE*" matches state 1 (CALIBRATE_BANDPASS.ON_SOURCE).
    // State 1: rows 4,5,6,9 → 4 rows.
    MsSelection sel;
    sel.set_state_expr("*CALIBRATE*");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 4);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_combined_selection() {
    std::cout << "  combined selection (AND logic)... ";

    auto path = make_temp_dir("combined");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Field 0 AND scan 1: field0 has rows 0,1,2,3,7,8; scan1 has same → 6 rows.
    MsSelection sel;
    sel.set_field_expr("0");
    sel.set_scan_expr("1");
    auto result = sel.evaluate(ms);
    assert(result.rows.size() == 6);

    // Field 1 AND antenna 3: field1 rows 4,5,6,9; ant3 rows 3,6,7 → intersection = {6}.
    MsSelection sel2;
    sel2.set_field_expr("1");
    sel2.set_antenna_expr("3");
    auto result2 = sel2.evaluate(ms);
    // Field 1: 4,5,6,9.
    // Antenna 3 in those: row 6 has ant1=1,ant2=3 → ant3 present → match.
    // Row 4: 0-1 → no ant3.
    // Row 5: 0-2 → no ant3.
    // Row 9: 0-1 → no ant3.
    assert(result2.rows.size() == 1);
    assert(result2.rows[0] == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_malformed_antenna_expr() {
    std::cout << "  malformed antenna expression... ";

    auto path = make_temp_dir("malfant");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "0&" → incomplete baseline.
    {
        MsSelection sel;
        sel.set_antenna_expr("0&");
        bool caught = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error& e) {
            caught = true;
            std::string msg = e.what();
            assert(msg.find("Antenna") != std::string::npos);
        }
        assert(caught);
    }

    // "abc~def" → non-numeric range.
    {
        MsSelection sel;
        sel.set_antenna_expr("abc~def");
        bool caught = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error& e) {
            caught = true;
        }
        assert(caught);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_malformed_spw_expr() {
    std::cout << "  malformed SPW expression... ";

    auto path = make_temp_dir("malfspw");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "0:" → missing channel specification.
    {
        MsSelection sel;
        sel.set_spw_expr("0:");
        bool caught = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error& e) {
            caught = true;
            std::string msg = e.what();
            assert(msg.find("SPW") != std::string::npos);
        }
        assert(caught);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_malformed_time_expr() {
    std::cout << "  malformed time expression... ";

    auto path = make_temp_dir("malftime");
    cleanup(path);
    auto ms = make_test_ms(path);

    {
        MsSelection sel;
        sel.set_time_expr("not-a-number");
        bool caught = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error& e) {
            caught = true;
        }
        assert(caught);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_empty_ms() {
    std::cout << "  selection on empty MS... ";

    auto path = make_temp_dir("emptyms");
    cleanup(path);

    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);
    writer.add_antenna({.name = "ANT0",
                        .station = {},
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 0.0,
                        .flag_row = false});
    writer.add_field(
        {.name = "F0", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.flush();

    auto ms2 = MeasurementSet::open(path);
    MsSelection sel;
    sel.set_antenna_expr("0");
    auto result = sel.evaluate(ms2);
    assert(result.rows.empty());

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_selection_test\n";

        test_no_selection_returns_all_rows();
        test_antenna_id_selection();
        test_antenna_range_selection();
        test_antenna_baseline_selection();
        test_antenna_wildcard_baseline();
        test_antenna_negation();
        test_antenna_by_name();
        test_field_id_selection();
        test_field_by_name();
        test_field_glob_pattern();
        test_spw_selection();
        test_spw_with_channels();
        test_spw_all_wildcard();
        test_scan_selection();
        test_scan_range();
        test_scan_negation();
        test_time_greater_than();
        test_time_range();
        test_uvdist_selection();
        test_uvdist_range();
        test_corr_selection();
        test_state_id_selection();
        test_state_pattern_selection();
        test_combined_selection();
        test_malformed_antenna_expr();
        test_malformed_spw_expr();
        test_malformed_time_expr();
        test_empty_ms();

        std::cout << "all ms_selection tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
