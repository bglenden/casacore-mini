/// @file ms_selection_malformed_test.cpp
/// @brief Malformed-input tests for W5 MS selection categories.

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

/// Minimal MS for malformed tests (just needs to exist with 1 row).
static MeasurementSet make_minimal_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    writer.add_antenna({.name = "A0",
                        .station = "P0",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});

    writer.add_field({.name = "F0",
                      .code = "T",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    writer.add_spectral_window({.num_chan = 4,
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
    writer.add_polarization({.num_corr = 1, .corr_type = {5}, .flag_row = false});

    writer.add_observation({.telescope_name = "VLA",
                            .observer = "test",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE_TARGET",
                      .flag_row = false});

    writer.add_row({.antenna1 = 0,
                    .antenna2 = 0,
                    .array_id = 0,
                    .data_desc_id = 0,
                    .exposure = 10.0,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = 0,
                    .flag_row = false,
                    .interval = 10.0,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = 1,
                    .state_id = 0,
                    .time = 4.8e9,
                    .time_centroid = 4.8e9,
                    .uvw = {0.0, 0.0, 0.0},
                    .sigma = {1.0F},
                    .weight = {1.0F},
                    .data = {},
                    .flag = {}});
    writer.flush();
    return MeasurementSet::open(path);
}

static bool expect_throw(MsSelection& sel, MeasurementSet& ms, const char* label) {
    try {
        (void)sel.evaluate(ms);
        check(false, label);
        return false;
    } catch (const std::runtime_error&) {
        check(true, label);
        return true;
    }
}

static void test_observation_malformed() {
    auto path = fs::temp_directory_path() / "mssel_mal_obs";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_minimal_ms(path);

    MsSelection sel;

    sel.clear();
    sel.set_observation_expr("");
    expect_throw(sel, ms, "observation: empty expression");

    sel.clear();
    sel.set_observation_expr("abc");
    expect_throw(sel, ms, "observation: non-numeric");

    sel.clear();
    sel.set_observation_expr(">abc");
    expect_throw(sel, ms, "observation: invalid bound");

    fs::remove_all(path);
}

static void test_array_malformed() {
    auto path = fs::temp_directory_path() / "mssel_mal_arr";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_minimal_ms(path);

    MsSelection sel;

    sel.clear();
    sel.set_array_expr("");
    expect_throw(sel, ms, "array: empty expression");

    sel.clear();
    sel.set_array_expr("xyz");
    expect_throw(sel, ms, "array: non-numeric");

    sel.clear();
    sel.set_array_expr("<");
    expect_throw(sel, ms, "array: incomplete bound");

    fs::remove_all(path);
}

static void test_feed_malformed() {
    auto path = fs::temp_directory_path() / "mssel_mal_feed";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_minimal_ms(path);

    MsSelection sel;

    sel.clear();
    sel.set_feed_expr("");
    expect_throw(sel, ms, "feed: empty expression");

    sel.clear();
    sel.set_feed_expr("&1");
    expect_throw(sel, ms, "feed: missing lhs");

    sel.clear();
    sel.set_feed_expr("abc&1");
    expect_throw(sel, ms, "feed: non-numeric lhs");

    sel.clear();
    sel.set_feed_expr("0&abc");
    expect_throw(sel, ms, "feed: non-numeric rhs");

    fs::remove_all(path);
}

static void test_taql_malformed() {
    auto path = fs::temp_directory_path() / "mssel_mal_taql";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_minimal_ms(path);

    MsSelection sel;

    sel.clear();
    sel.set_taql_expr("");
    expect_throw(sel, ms, "taql: empty expression");

    fs::remove_all(path);
}

static void test_scan_bounds_malformed() {
    auto path = fs::temp_directory_path() / "mssel_mal_scan_bnd";
    if (fs::exists(path)) fs::remove_all(path);
    auto ms = make_minimal_ms(path);

    MsSelection sel;

    sel.clear();
    sel.set_scan_expr("<abc");
    expect_throw(sel, ms, "scan: invalid bound value");

    sel.clear();
    sel.set_scan_expr(">");
    expect_throw(sel, ms, "scan: empty bound");

    fs::remove_all(path);
}

int main() { // NOLINT(bugprone-exception-escape)
    test_observation_malformed();
    test_array_malformed();
    test_feed_malformed();
    test_taql_malformed();
    test_scan_bounds_malformed();

    std::cout << "ms_selection_malformed_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
