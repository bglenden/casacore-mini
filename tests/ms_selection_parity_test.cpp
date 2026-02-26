#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"
#include "casacore_mini/measurement_set.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_parity_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Build a multi-field, multi-SPW, multi-scan stress-test MS.
/// 6 antennas, 3 fields, 3 SPWs, 3 scans, 3 states, 2 array IDs.
/// 30 rows with varied combinations.
static MeasurementSet make_stress_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    for (int i = 0; i < 6; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
    }

    writer.add_field({.name = "3C273", .code = "T", .time = 0.0, .num_poly = 0,
                       .source_id = -1, .flag_row = false});
    writer.add_field({.name = "M31", .code = "T", .time = 0.0, .num_poly = 0,
                       .source_id = -1, .flag_row = false});
    writer.add_field({.name = "CasA", .code = "C", .time = 0.0, .num_poly = 0,
                       .source_id = -1, .flag_row = false});

    writer.add_spectral_window({.num_chan = 64, .name = "L-band",
                                 .ref_frequency = 1.4e9, .chan_freq = {},
                                 .chan_width = {}, .effective_bw = {},
                                 .resolution = {}, .meas_freq_ref = 0,
                                 .total_bandwidth = 0.0, .net_sideband = 0,
                                 .if_conv_chain = 0, .freq_group = 0,
                                 .freq_group_name = {}, .flag_row = false});
    writer.add_spectral_window({.num_chan = 128, .name = "S-band",
                                 .ref_frequency = 3.0e9, .chan_freq = {},
                                 .chan_width = {}, .effective_bw = {},
                                 .resolution = {}, .meas_freq_ref = 0,
                                 .total_bandwidth = 0.0, .net_sideband = 0,
                                 .if_conv_chain = 0, .freq_group = 0,
                                 .freq_group_name = {}, .flag_row = false});
    writer.add_spectral_window({.num_chan = 256, .name = "C-band",
                                 .ref_frequency = 6.0e9, .chan_freq = {},
                                 .chan_width = {}, .effective_bw = {},
                                 .resolution = {}, .meas_freq_ref = 0,
                                 .total_bandwidth = 0.0, .net_sideband = 0,
                                 .if_conv_chain = 0, .freq_group = 0,
                                 .freq_group_name = {}, .flag_row = false});

    // DD0→SPW0, DD1→SPW1, DD2→SPW2
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0,
                                  .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0,
                                  .flag_row = false});
    writer.add_data_description({.spectral_window_id = 2, .polarization_id = 0,
                                  .flag_row = false});

    writer.add_polarization({.num_corr = 4, .corr_type = {5, 6, 7, 8},
                              .flag_row = false});

    writer.add_observation({.telescope_name = "VLA", .observer = "test",
                             .project = {}, .release_date = 0.0,
                             .flag_row = false});

    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0,
                       .sub_scan = 0, .obs_mode = "OBSERVE_TARGET.ON_SOURCE",
                       .flag_row = false});
    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0,
                       .sub_scan = 0, .obs_mode = "CALIBRATE_BANDPASS.ON_SOURCE",
                       .flag_row = false});
    writer.add_state({.sig = true, .ref = false, .cal = 0.0, .load = 0.0,
                       .sub_scan = 0, .obs_mode = "CALIBRATE_PHASE.ON_SOURCE",
                       .flag_row = false});

    // 30 rows with systematic variation.
    double base_time = 4.8e9;
    int row_count = 0;
    for (int scan = 1; scan <= 3; ++scan) {
        for (int field_id = 0; field_id < 3; ++field_id) {
            // Two baselines per field per scan, plus one more for variety.
            int ant1_vals[] = {0, 1, 2};
            int ant2_vals[] = {3, 4, 5};
            int dd_id = field_id;  // field i uses DD i → SPW i
            int state_id = scan - 1;
            int array_id = (scan <= 2) ? 0 : 1;

            for (int bi = 0; bi < 3; ++bi) {
                double t = base_time + static_cast<double>(row_count) * 10.0;
                double u = 100.0 * static_cast<double>(row_count + 1);
                double v = 50.0 * static_cast<double>(row_count + 1);

                writer.add_row({
                    .antenna1 = ant1_vals[bi],
                    .antenna2 = ant2_vals[bi],
                    .array_id = array_id,
                    .data_desc_id = dd_id,
                    .exposure = 0.0,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = field_id,
                    .flag_row = false,
                    .interval = 10.0,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = scan,
                    .state_id = state_id,
                    .time = t,
                    .time_centroid = t,
                    .uvw = {u, v, 0.0},
                    .sigma = {1.0F, 1.0F, 1.0F, 1.0F},
                    .weight = {1.0F, 1.0F, 1.0F, 1.0F},
                    .data = {},
                    .flag = {}
                });
                ++row_count;
            }
        }
    }

    assert(row_count == 27);
    writer.flush();
    return MeasurementSet::open(path);
}

// ---------------------------------------------------------------------------
// Per-category parity tests
// ---------------------------------------------------------------------------

static void test_antenna_parity() {
    std::cout << "  antenna parity: multi-id, range, baseline, negate... ";

    auto path = make_temp_dir("ant_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // "0,1,2" → all rows (every row has ant1 in {0,1,2}).
    {
        MsSelection sel;
        sel.set_antenna_expr("0,1,2");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 27);
    }

    // "3,4,5" → all rows (every row has ant2 in {3,4,5}).
    {
        MsSelection sel;
        sel.set_antenna_expr("3,4,5");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 27);
    }

    // "0&3" → rows where (ant1=0,ant2=3) or (ant1=3,ant2=0).
    // Pattern: ant1_vals[0]=0, ant2_vals[0]=3, so bi=0 for each scan/field.
    // 3 scans * 3 fields = 9 rows.
    {
        MsSelection sel;
        sel.set_antenna_expr("0&3");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 9);
    }

    // "!0" → rows where neither ant1 nor ant2 is 0.
    // Row ant1 is 0 when bi=0 → 9 rows have ant1=0. Remove those → 18 rows.
    {
        MsSelection sel;
        sel.set_antenna_expr("!0");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 18);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_field_parity() {
    std::cout << "  field parity: by id, name, glob... ";

    auto path = make_temp_dir("fld_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Each field appears in 3 scans * 3 baselines = 9 rows.
    {
        MsSelection sel;
        sel.set_field_expr("0");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    {
        MsSelection sel;
        sel.set_field_expr("M31");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    {
        MsSelection sel;
        sel.set_field_expr("Cas*");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    // Multi-field: "0,2"
    {
        MsSelection sel;
        sel.set_field_expr("0,2");
        assert(sel.evaluate(ms).rows.size() == 18);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_parity() {
    std::cout << "  SPW parity: by id, channel ranges, wildcard... ";

    auto path = make_temp_dir("spw_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Each SPW → one DD → one field per scan → 3 scans * 3 baselines = 9 rows.
    {
        MsSelection sel;
        sel.set_spw_expr("0");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    {
        MsSelection sel;
        sel.set_spw_expr("1");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    // "0,1" → 18 rows.
    {
        MsSelection sel;
        sel.set_spw_expr("0,1");
        assert(sel.evaluate(ms).rows.size() == 18);
    }
    // Channel range: "0:0~31" → same rows as SPW 0.
    {
        MsSelection sel;
        sel.set_spw_expr("0:0~31");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 9);
        assert(r.channel_ranges.size() == 3); // {spw=0, start=0, end=31}
    }
    // Wildcard: all rows.
    {
        MsSelection sel;
        sel.set_spw_expr("*");
        assert(sel.evaluate(ms).rows.size() == 27);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_scan_parity() {
    std::cout << "  scan parity: id, range, negate... ";

    auto path = make_temp_dir("scan_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Each scan has 3 fields * 3 baselines = 9 rows.
    {
        MsSelection sel;
        sel.set_scan_expr("1");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    // Range "1~2" → 18 rows.
    {
        MsSelection sel;
        sel.set_scan_expr("1~2");
        assert(sel.evaluate(ms).rows.size() == 18);
    }
    // "!3" → exclude scan 3 → 18 rows.
    {
        MsSelection sel;
        sel.set_scan_expr("!3");
        assert(sel.evaluate(ms).rows.size() == 18);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_time_parity() {
    std::cout << "  time parity: >, <, range... ";

    auto path = make_temp_dir("time_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Rows at times: 4.8e9 + 0, 10, 20, 30, 40, 50, 60, ..., 260.
    // First 9 rows (scan 1): times 4.8e9+0..80.
    // Next 9 rows (scan 2): times 4.8e9+90..170.
    // Last 9 rows (scan 3): times 4.8e9+180..260.

    // ">4800000100" → times 110, 120, ..., 260 → rows 11..26 → 16 rows.
    {
        MsSelection sel;
        sel.set_time_expr(">4800000100");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 16);
    }

    // "<4800000100" → times 0, 10, ..., 90 → rows 0..9 → 10 rows.
    {
        MsSelection sel;
        sel.set_time_expr("<4800000100");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 10);
    }

    // Range "4800000050~4800000150" → times 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150.
    // That's rows 5..15 → 11 rows.
    {
        MsSelection sel;
        sel.set_time_expr("4800000050~4800000150");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 11);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_uvdist_parity() {
    std::cout << "  UV-distance parity: >, range... ";

    auto path = make_temp_dir("uv_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // UVW: u=100*(row+1), v=50*(row+1), w=0.
    // uvdist = sqrt(u^2+v^2) = sqrt(10000*(row+1)^2 + 2500*(row+1)^2)
    //        = (row+1) * sqrt(12500) ≈ (row+1) * 111.8.
    // Row 0: 111.8, row 1: 223.6, ..., row 26: 3018.6.

    // ">1000" → (row+1)*111.8 > 1000 → row+1 > 8.94 → row >= 8 → 19 rows.
    {
        MsSelection sel;
        sel.set_uvdist_expr(">1000");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 19);
    }

    // "500~1500" → (row+1)*111.8 in [500,1500]
    // row+1 in [4.47, 13.41] → row+1 = 5..13 → row = 4..12 → 9 rows.
    {
        MsSelection sel;
        sel.set_uvdist_expr("500~1500");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 9);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_corr_parity() {
    std::cout << "  correlation parity: names, indices... ";

    auto path = make_temp_dir("corr_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Correlation selection doesn't filter rows, just records the selection.
    {
        MsSelection sel;
        sel.set_corr_expr("RR,LL");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 27);
        assert(r.correlations.size() == 2);
    }
    {
        MsSelection sel;
        sel.set_corr_expr("I,Q,U,V");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 27);
        assert(r.correlations.size() == 4);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_state_parity() {
    std::cout << "  state parity: id, pattern... ";

    auto path = make_temp_dir("state_parity");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // State 0 = scan 1 → 9 rows.
    {
        MsSelection sel;
        sel.set_state_expr("0");
        assert(sel.evaluate(ms).rows.size() == 9);
    }
    // "*CALIBRATE*" matches states 1 and 2 → scans 2 and 3 → 18 rows.
    {
        MsSelection sel;
        sel.set_state_expr("*CALIBRATE*");
        assert(sel.evaluate(ms).rows.size() == 18);
    }
    // "*PHASE*" matches state 2 only → scan 3 → 9 rows.
    {
        MsSelection sel;
        sel.set_state_expr("*PHASE*");
        assert(sel.evaluate(ms).rows.size() == 9);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_mixed_expression_and_logic() {
    std::cout << "  mixed expression AND logic... ";

    auto path = make_temp_dir("mixed");
    cleanup(path);
    auto ms = make_stress_ms(path);

    // Field 0 AND scan 1 AND antenna 0&3:
    // Field 0: 9 rows (rows 0..2 in scan1, 9..11 in scan2, 18..20 in scan3).
    // Scan 1: rows 0..8.
    // Intersection field0+scan1: rows 0,1,2.
    // Antenna 0&3: bi=0 within each group → rows 0, 3, 6, 9, 12, ...
    // Within rows 0,1,2: row 0 has ant0&3 → match. Rows 1,2 have other baselines.
    // Result: row 0 only.
    {
        MsSelection sel;
        sel.set_field_expr("0");
        sel.set_scan_expr("1");
        sel.set_antenna_expr("0&3");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 1);
        assert(r.rows[0] == 0);
    }

    // Field "CasA" AND SPW 2 AND scan "2~3":
    // Field 2 (CasA): rows 6..8 in scan1, 15..17 in scan2, 24..26 in scan3.
    // SPW 2 (DD2): same as field 2 because dd_id = field_id.
    // Scan 2~3: rows 9..26.
    // Intersection: {15,16,17, 24,25,26} → 6 rows.
    {
        MsSelection sel;
        sel.set_field_expr("CasA");
        sel.set_spw_expr("2");
        sel.set_scan_expr("2~3");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 6);
    }

    // Three-way: field 0 AND spw 0 AND time < 4800000050:
    // Field 0 + SPW 0: rows 0,1,2 (scan1), 9,10,11 (scan2), 18,19,20 (scan3).
    // Time < 4800000050: rows 0..4 (times 0, 10, 20, 30, 40).
    // Intersection: rows 0,1,2 → 3 rows.
    {
        MsSelection sel;
        sel.set_field_expr("0");
        sel.set_spw_expr("0");
        sel.set_time_expr("<4800000050");
        auto r = sel.evaluate(ms);
        assert(r.rows.size() == 3);
    }

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_malformed_per_category() {
    std::cout << "  malformed expressions per category... ";

    auto path = make_temp_dir("malf");
    cleanup(path);
    auto ms = make_stress_ms(path);

    auto expect_throw = [&](auto set_fn) {
        MsSelection sel;
        set_fn(sel);
        bool caught = false;
        try {
            (void)sel.evaluate(ms);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        assert(caught);
    };

    // Antenna: incomplete baseline.
    expect_throw([](MsSelection& s) { s.set_antenna_expr("0&"); });
    // Antenna: non-numeric range.
    expect_throw([](MsSelection& s) { s.set_antenna_expr("abc~xyz"); });
    // Field: empty.
    expect_throw([](MsSelection& s) { s.set_field_expr(""); });
    // SPW: missing channels after colon.
    expect_throw([](MsSelection& s) { s.set_spw_expr("0:"); });
    // SPW: non-numeric.
    expect_throw([](MsSelection& s) { s.set_spw_expr("abc"); });
    // Scan: non-numeric range.
    expect_throw([](MsSelection& s) { s.set_scan_expr("a~b"); });
    // Time: not a number.
    expect_throw([](MsSelection& s) { s.set_time_expr("not-a-date"); });
    // UVdist: non-numeric.
    expect_throw([](MsSelection& s) { s.set_uvdist_expr("abc"); });

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
    std::cout << "ms_selection_parity_test\n";

    test_antenna_parity();
    test_field_parity();
    test_spw_parity();
    test_scan_parity();
    test_time_parity();
    test_uvdist_parity();
    test_corr_parity();
    test_state_parity();
    test_mixed_expression_and_logic();
    test_malformed_per_category();

    std::cout << "all ms_selection_parity tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
