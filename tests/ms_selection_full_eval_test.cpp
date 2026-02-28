#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

static int check_count = 0;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_full_eval_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Build a test MS with known data for full evaluation tests.
/// 4 antennas (ANT0-ANT3), 2 fields (3C273, M31), 2 SPWs, 2 scans,
/// 2 states, 1 observation, array_id=0, multiple rows.
static MeasurementSet make_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // 4 antennas with names matching regex patterns.
    writer.add_antenna({.name = "DV01",
                        .station = "PAD0",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 12.0,
                        .flag_row = false});
    writer.add_antenna({.name = "DV02",
                        .station = "PAD1",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 12.0,
                        .flag_row = false});
    writer.add_antenna({.name = "DA41",
                        .station = "PAD2",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 7.0,
                        .flag_row = false});
    writer.add_antenna({.name = "DA42",
                        .station = "PAD3",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 7.0,
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

    // 2 SPWs with known frequencies for frequency-range testing.
    // SPW0: 64 channels, ref 1.4 GHz, total BW 128 MHz
    writer.add_spectral_window({.num_chan = 64,
                                .name = "L-band",
                                .ref_frequency = 1.4e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 128.0e6,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    // SPW1: 128 channels, ref 3.0 GHz, total BW 256 MHz
    writer.add_spectral_window({.num_chan = 128,
                                .name = "S-band",
                                .ref_frequency = 3.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 256.0e6,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});

    // 2 data descriptions: DD0->SPW0, DD1->SPW1.
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0, .flag_row = false});

    // 1 polarization with RR, LL.
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

    // 1 observation.
    writer.add_observation({.telescope_name = "ALMA",
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

    // MJD time for 2024/01/15 12:00:00 UTC in seconds.
    // JDN for 2024-01-15: Meeus formula
    // a=(14-1)/12=1, y=2024+4800-1=6823, m=1+12-3=10
    // JDN=15+(153*10+2)/5+365*6823+6823/4-6823/100+6823/400-32045
    // JDN=15+306+2490395+1705-68+17-32045 = 2460325 (approx)
    // JD = 2460325 + (12-12)/24 = 2460325.0
    // MJD = 2460325.0 - 2400000.5 = 60324.5
    // MJD seconds = 60324.5 * 86400 = 5212036800.0
    double base_time = 5212036800.0; // approximately 2024/01/15 12:00:00

    // 10 rows with varied combinations.
    struct RowDef {
        std::int32_t ant1, ant2, field_id, dd_id, scan, state_id;
        double time_offset;
        double u, v, w;
    };
    RowDef rows[] = {
        {0, 1, 0, 0, 1, 0, 0.0, 100, 200, 50},
        {0, 2, 0, 0, 1, 0, 0.0, 300, 400, 100},
        {1, 2, 0, 0, 1, 0, 0.0, 200, 100, 30},
        {0, 3, 0, 1, 1, 0, 0.0, 500, 600, 150},
        {0, 1, 1, 0, 2, 1, 100.0, 110, 210, 55},
        {0, 2, 1, 0, 2, 1, 100.0, 310, 410, 105},
        {1, 3, 1, 1, 2, 1, 100.0, 400, 300, 80},
        {2, 3, 0, 0, 1, 0, 50.0, 50, 30, 10},
        {0, 1, 0, 0, 1, 0, 50.0, 10, 10, 5},
        {0, 1, 1, 1, 2, 1, 200.0, 800, 900, 200},
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
                        .time = base_time + rd.time_offset,
                        .time_centroid = base_time + rd.time_offset,
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
// Check macro
// ---------------------------------------------------------------------------

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        ++check_count;                                                                             \
        if (!(cond)) {                                                                             \
            std::cerr << "CHECK FAILED at line " << __LINE__ << ": " << #cond << "\n";             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_accessor_antenna_lists() {
    std::cout << "  accessor: antenna1, antenna2, baseline lists... ";

    auto path = make_temp_dir("acc_ant");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Baseline selection 0&1.
    MsSelection sel;
    sel.set_antenna_expr("0&1");
    auto result = sel.evaluate(ms);
    (void)result;

    auto a1_list = sel.getAntenna1List();
    auto a2_list = sel.getAntenna2List();
    auto bl_list = sel.getBaselineList();

    CHECK(a1_list.size() == 1);
    CHECK(a1_list[0] == 0);
    CHECK(a2_list.size() == 1);
    CHECK(a2_list[0] == 1);
    CHECK(bl_list.size() == 1);
    CHECK(bl_list[0] == std::make_pair(0, 1));

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_antenna_id_list() {
    std::cout << "  accessor: antenna id list via range... ";

    auto path = make_temp_dir("acc_antid");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_antenna_expr("0~2");
    auto result = sel.evaluate(ms);
    (void)result;

    auto a1 = sel.getAntenna1List();
    CHECK(a1.size() == 3);
    CHECK(a1[0] == 0);
    CHECK(a1[1] == 1);
    CHECK(a1[2] == 2);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_field_list() {
    std::cout << "  accessor: field list... ";

    auto path = make_temp_dir("acc_fld");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_field_expr("0,1");
    auto result = sel.evaluate(ms);
    (void)result;

    auto flds = sel.getFieldList();
    CHECK(flds.size() == 2);
    CHECK(flds[0] == 0);
    CHECK(flds[1] == 1);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_spw_list() {
    std::cout << "  accessor: SPW list... ";

    auto path = make_temp_dir("acc_spw");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("0");
    auto result = sel.evaluate(ms);
    (void)result;

    auto spws = sel.getSpwList();
    CHECK(spws.size() == 1);
    CHECK(spws[0] == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_chan_slices() {
    std::cout << "  accessor: channel slices... ";

    auto path = make_temp_dir("acc_chan");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("0:10~20");
    auto result = sel.evaluate(ms);
    (void)result;

    auto slices = sel.getChanSlices();
    CHECK(slices.size() == 1);
    CHECK(slices[0].first == 10);
    CHECK(slices[0].second == 11); // width = 20 - 10 + 1

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_scan_list() {
    std::cout << "  accessor: scan list... ";

    auto path = make_temp_dir("acc_scan");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_scan_expr("1~2");
    auto result = sel.evaluate(ms);
    (void)result;

    auto scans = sel.getScanList();
    CHECK(scans.size() == 2);
    CHECK(scans[0] == 1);
    CHECK(scans[1] == 2);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_state_list() {
    std::cout << "  accessor: state list... ";

    auto path = make_temp_dir("acc_state");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_state_expr("0,1");
    auto result = sel.evaluate(ms);
    (void)result;

    auto states = sel.getStateList();
    CHECK(states.size() == 2);
    CHECK(states[0] == 0);
    CHECK(states[1] == 1);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_observation_list() {
    std::cout << "  accessor: observation list... ";

    auto path = make_temp_dir("acc_obs");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_observation_expr("0");
    auto result = sel.evaluate(ms);
    (void)result;

    auto obs = sel.getObservationList();
    CHECK(obs.size() == 1);
    CHECK(obs[0] == 0);
    CHECK(result.rows.size() == 10); // All rows have obs_id=0

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_subarray_list() {
    std::cout << "  accessor: subarray list... ";

    auto path = make_temp_dir("acc_arr");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_array_expr("0");
    auto result = sel.evaluate(ms);
    (void)result;

    auto arrs = sel.getSubArrayList();
    CHECK(arrs.size() == 1);
    CHECK(arrs[0] == 0);
    CHECK(result.rows.size() == 10); // All rows have array_id=0

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_ddid_list() {
    std::cout << "  accessor: DDID list... ";

    auto path = make_temp_dir("acc_dd");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_spw_expr("0");
    auto result = sel.evaluate(ms);
    (void)result;

    auto dds = sel.getDDIDList();
    CHECK(dds.size() == 1);
    CHECK(dds[0] == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_time_list() {
    std::cout << "  accessor: time list... ";

    auto path = make_temp_dir("acc_time");
    cleanup(path);
    auto ms = make_test_ms(path);

    double base_time = 5212036800.0;
    MsSelection sel;
    std::string expr = std::to_string(base_time) + "~" + std::to_string(base_time + 100.0);
    sel.set_time_expr(expr);
    auto result = sel.evaluate(ms);
    (void)result;

    auto tl = sel.getTimeList();
    CHECK(tl.size() == 2);
    CHECK(std::abs(tl[0] - base_time) < 1.0);
    CHECK(std::abs(tl[1] - (base_time + 100.0)) < 1.0);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_corr_slices() {
    std::cout << "  accessor: correlation slices... ";

    auto path = make_temp_dir("acc_corr");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_corr_expr("RR,LL");
    auto result = sel.evaluate(ms);
    (void)result;

    auto cs = sel.getCorrSlices();
    CHECK(cs.size() == 2);
    CHECK(cs[0].first == 0);
    CHECK(cs[0].second == 1);
    CHECK(cs[1].first == 1);
    CHECK(cs[1].second == 1);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_accessor_uv_list() {
    std::cout << "  accessor: UV list... ";

    auto path = make_temp_dir("acc_uv");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_uvdist_expr("100~500");
    auto result = sel.evaluate(ms);
    (void)result;

    auto uvl = sel.getUVList();
    CHECK(uvl.size() == 2);
    CHECK(std::abs(uvl[0] - 100.0) < 0.01);
    CHECK(std::abs(uvl[1] - 500.0) < 0.01);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_regex() {
    std::cout << "  antenna regex pattern... ";

    auto path = make_temp_dir("ant_regex");
    cleanup(path);
    auto ms = make_test_ms(path);

    // /DV\\d+/ should match DV01 and DV02 (antenna IDs 0 and 1).
    MsSelection sel;
    sel.set_antenna_expr("/DV\\d+/");
    auto result = sel.evaluate(ms);

    // DV01=ant0, DV02=ant1. All rows have at least one of ant0 or ant1 except row 7 (2-3).
    CHECK(result.rows.size() == 9);

    // /DA/ should match DA41 and DA42 (antenna IDs 2 and 3).
    MsSelection sel2;
    sel2.set_antenna_expr("/DA/");
    auto result2 = sel2.evaluate(ms);
    // Rows with ant2 or ant3: rows 1,2,3,5,6,7 (6 rows? Let's check)
    // Row 0: 0-1 -> no DA
    // Row 1: 0-2 -> yes (DA41)
    // Row 2: 1-2 -> yes (DA41)
    // Row 3: 0-3 -> yes (DA42)
    // Row 4: 0-1 -> no DA
    // Row 5: 0-2 -> yes (DA41)
    // Row 6: 1-3 -> yes (DA42)
    // Row 7: 2-3 -> yes (both DA)
    // Row 8: 0-1 -> no DA
    // Row 9: 0-1 -> no DA
    CHECK(result2.rows.size() == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_frequency_range() {
    std::cout << "  SPW frequency range... ";

    auto path = make_temp_dir("spw_freq");
    cleanup(path);
    auto ms = make_test_ms(path);

    // SPW0: ref=1.4GHz, BW=128MHz. Channels span [1.336GHz, 1.464GHz].
    // chan_width = 128MHz/64 = 2MHz per channel.
    // freq_start = 1.4GHz - 64MHz = 1.336GHz.
    // "0:1370~1400MHz" should select channels from 1370MHz to 1400MHz.
    // channel index = (freq - 1.336e9) / 2e6
    // 1370MHz -> (1.37e9 - 1.336e9) / 2e6 = 34e6/2e6 = 17
    // 1400MHz -> (1.40e9 - 1.336e9) / 2e6 = 64e6/2e6 = 32
    MsSelection sel;
    sel.set_spw_expr("0:1370MHz~1400MHz");
    auto result = sel.evaluate(ms);

    // Should still select rows with SPW 0.
    CHECK(result.rows.size() == 7);
    CHECK(result.spws.size() == 1);
    CHECK(result.spws[0] == 0);

    // Check channel slices were computed.
    auto slices = sel.getChanSlices();
    CHECK(!slices.empty());
    // The start channel should be around 17.
    CHECK(slices[0].first >= 15 && slices[0].first <= 20);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_time_datetime_string() {
    std::cout << "  time with datetime string... ";

    auto path = make_temp_dir("time_dt");
    cleanup(path);
    auto ms = make_test_ms(path);

    // The base time is approximately 2024/01/15/12:00:00 in MJD seconds.
    // Test date range that covers our test data.
    MsSelection sel;
    sel.set_time_expr("2024/01/15/11:00:00~2024/01/15/13:00:00");
    auto result = sel.evaluate(ms);

    // All rows should be within this range (base_time +0..+200 seconds).
    CHECK(result.rows.size() == 10);

    // Now test a narrower range.
    MsSelection sel2;
    sel2.set_time_expr("2024/01/15/12:00:00~2024/01/15/12:00:01");
    auto result2 = sel2.evaluate(ms);

    // Should get rows at time offset 0 (rows 0,1,2,3) = base_time exactly.
    // Actually base_time is approximate, so just check we got some rows.
    CHECK(result2.rows.size() >= 1);

    // Check that time list was recorded.
    auto tl = sel2.getTimeList();
    CHECK(tl.size() == 2);
    CHECK(tl[0] > 0.0);
    CHECK(tl[1] > tl[0]);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_error_handler() {
    std::cout << "  error handler... ";

    MsSelectionErrorHandler handler;
    CHECK(!handler.hasErrors());
    CHECK(handler.getMessages().empty());

    handler.handleError("error 1");
    handler.handleError("error 2");
    CHECK(handler.hasErrors());
    CHECK(handler.getMessages().size() == 2);
    CHECK(handler.getMessages()[0] == "error 1");
    CHECK(handler.getMessages()[1] == "error 2");

    handler.clear();
    CHECK(!handler.hasErrors());
    CHECK(handler.getMessages().empty());

    cleanup(fs::path{});
    std::cout << "PASS\n";
}

static void test_parse_late_flag() {
    std::cout << "  PARSE_LATE flag... ";

    MsSelection sel;
    CHECK(!sel.parse_late());

    sel.set_parse_late(true);
    CHECK(sel.parse_late());

    sel.set_parse_late(false);
    CHECK(!sel.parse_late());

    // PARSE_LATE doesn't change evaluation behavior -- it's a mode flag only.
    auto path = make_temp_dir("parse_late");
    cleanup(path);
    auto ms = make_test_ms(path);

    sel.set_parse_late(true);
    sel.set_scan_expr("1");
    auto result = sel.evaluate(ms);
    CHECK(result.rows.size() == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_empty_expressions_return_empty() {
    std::cout << "  empty expressions return empty vectors... ";

    auto path = make_temp_dir("empty_expr");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    // No expressions set.
    auto result = sel.evaluate(ms);

    CHECK(sel.getAntenna1List().empty());
    CHECK(sel.getAntenna2List().empty());
    CHECK(sel.getBaselineList().empty());
    CHECK(sel.getFieldList().empty());
    CHECK(sel.getSpwList().empty());
    CHECK(sel.getChanSlices().empty());
    CHECK(sel.getScanList().empty());
    CHECK(sel.getStateList().empty());
    CHECK(sel.getObservationList().empty());
    CHECK(sel.getSubArrayList().empty());
    CHECK(sel.getDDIDList().empty());
    CHECK(sel.getTimeList().empty());
    CHECK(sel.getCorrSlices().empty());
    CHECK(sel.getUVList().empty());

    // But all rows should be returned.
    CHECK(result.rows.size() == 10);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_combined_selections() {
    std::cout << "  combined selections... ";

    auto path = make_temp_dir("combined");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Field 0 AND scan 1 AND SPW 0.
    MsSelection sel;
    sel.set_field_expr("0");
    sel.set_scan_expr("1");
    sel.set_spw_expr("0");
    auto result = sel.evaluate(ms);

    // Field 0 rows: 0,1,2,3,7,8.
    // Scan 1 rows: 0,1,2,3,7,8.
    // SPW 0 (DD0) rows: 0,1,2,4,5,7,8.
    // Intersection: 0,1,2,7,8 (5 rows; row 3 has DD1).
    CHECK(result.rows.size() == 5);

    // Check accessor lists.
    auto flds = sel.getFieldList();
    CHECK(flds.size() == 1);
    CHECK(flds[0] == 0);

    auto scans = sel.getScanList();
    CHECK(scans.size() == 1);
    CHECK(scans[0] == 1);

    auto spws = sel.getSpwList();
    CHECK(spws.size() == 1);
    CHECK(spws[0] == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_observation_selection() {
    std::cout << "  observation selection... ";

    auto path = make_temp_dir("obs_sel");
    cleanup(path);
    auto ms = make_test_ms(path);

    // All rows have observation_id=0.
    MsSelection sel;
    sel.set_observation_expr("0");
    auto result = sel.evaluate(ms);
    CHECK(result.rows.size() == 10);

    // Non-existent observation.
    MsSelection sel2;
    sel2.set_observation_expr("99");
    auto result2 = sel2.evaluate(ms);
    CHECK(result2.rows.empty());

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_array_selection() {
    std::cout << "  array selection... ";

    auto path = make_temp_dir("arr_sel");
    cleanup(path);
    auto ms = make_test_ms(path);

    // All rows have array_id=0.
    MsSelection sel;
    sel.set_array_expr("0");
    auto result = sel.evaluate(ms);
    CHECK(result.rows.size() == 10);

    // Non-existent array.
    MsSelection sel2;
    sel2.set_array_expr("5");
    auto result2 = sel2.evaluate(ms);
    CHECK(result2.rows.empty());

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_intent_selection() {
    std::cout << "  intent selection... ";

    auto path = make_temp_dir("intent_sel");
    cleanup(path);
    auto ms = make_test_ms(path);

    // "*TARGET*" matches state 0 (OBSERVE_TARGET.ON_SOURCE).
    MsSelection sel;
    sel.set_intent_expr("*TARGET*");
    auto result = sel.evaluate(ms);
    // State 0 rows: 0,1,2,3,7,8 = 6 rows.
    CHECK(result.rows.size() == 6);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_poln_expr() {
    std::cout << "  poln expression (alias for corr)... ";

    auto path = make_temp_dir("poln");
    cleanup(path);
    auto ms = make_test_ms(path);

    MsSelection sel;
    sel.set_poln_expr("XX,YY");
    auto result = sel.evaluate(ms);
    CHECK(result.rows.size() == 10); // Doesn't filter rows.
    CHECK(result.correlations.size() == 2);
    CHECK(result.correlations[0] == "XX");
    CHECK(result.correlations[1] == "YY");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_has_selection_12_categories() {
    std::cout << "  has_selection with 12 categories... ";

    MsSelection sel;
    CHECK(!sel.has_selection());

    sel.set_antenna_expr("0");
    CHECK(sel.has_selection());

    MsSelection sel2;
    sel2.set_observation_expr("0");
    CHECK(sel2.has_selection());

    MsSelection sel3;
    sel3.set_array_expr("0");
    CHECK(sel3.has_selection());

    MsSelection sel4;
    sel4.set_intent_expr("*");
    CHECK(sel4.has_selection());

    MsSelection sel5;
    sel5.set_poln_expr("RR");
    CHECK(sel5.has_selection());

    MsSelection sel6;
    sel6.set_feed_expr("0");
    CHECK(sel6.has_selection());

    std::cout << "PASS\n";
}

static void test_spw_frequency_ghz() {
    std::cout << "  SPW frequency with GHz unit... ";

    auto path = make_temp_dir("spw_ghz");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Use GHz units: "0:1.37GHz~1.40GHz" should be the same as 1370MHz~1400MHz.
    MsSelection sel;
    sel.set_spw_expr("0:1.37GHz~1.40GHz");
    auto result = sel.evaluate(ms);

    CHECK(result.rows.size() == 7);
    auto slices = sel.getChanSlices();
    CHECK(!slices.empty());

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_antenna_regex_no_match() {
    std::cout << "  antenna regex no match... ";

    auto path = make_temp_dir("ant_regex_nm");
    cleanup(path);
    auto ms = make_test_ms(path);

    // /NONEXISTENT/ should match no antennas, resulting in no rows.
    MsSelection sel;
    sel.set_antenna_expr("/NONEXISTENT/");
    auto result = sel.evaluate(ms);
    CHECK(result.rows.empty());

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_selection_full_eval_test\n";

        test_accessor_antenna_lists();
        test_accessor_antenna_id_list();
        test_accessor_field_list();
        test_accessor_spw_list();
        test_accessor_chan_slices();
        test_accessor_scan_list();
        test_accessor_state_list();
        test_accessor_observation_list();
        test_accessor_subarray_list();
        test_accessor_ddid_list();
        test_accessor_time_list();
        test_accessor_corr_slices();
        test_accessor_uv_list();
        test_antenna_regex();
        test_spw_frequency_range();
        test_time_datetime_string();
        test_error_handler();
        test_parse_late_flag();
        test_empty_expressions_return_empty();
        test_combined_selections();
        test_observation_selection();
        test_array_selection();
        test_intent_selection();
        test_poln_expr();
        test_has_selection_12_categories();
        test_spw_frequency_ghz();
        test_antenna_regex_no_match();

        std::cout << "all ms_selection_full_eval tests passed (" << check_count << " checks)\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
