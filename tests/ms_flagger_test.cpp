#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/ms_flagger.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_flag_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static MeasurementSet make_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    writer.add_antenna({.name = "ANT0",
                        .station = "",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT1",
                        .station = "",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_field({.name = "3C273",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_spectral_window({.num_chan = 64,
                                .name = "L-band",
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
    writer.add_observation({.telescope_name = "VLA",
                            .observer = {},
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.obs_mode = "OBSERVE"});

    for (int i = 0; i < 5; ++i) {
        writer.add_row({.antenna1 = 0,
                        .antenna2 = 1,
                        .array_id = 0,
                        .data_desc_id = 0,
                        .exposure = 0.0,
                        .feed1 = 0,
                        .feed2 = 0,
                        .field_id = 0,
                        .flag_row = false,
                        .interval = 0.0,
                        .observation_id = 0,
                        .processor_id = 0,
                        .scan_number = 1,
                        .state_id = 0,
                        .time = 4.8e9 + static_cast<double>(i) * 10.0,
                        .time_centroid = 4.8e9 + static_cast<double>(i) * 10.0,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
    }
    writer.flush();
    return MeasurementSet::open(path);
}

static void test_initial_flag_stats() {
    std::cout << "  initial flag stats (all unflagged)... ";

    auto path = make_temp_dir("stats");
    cleanup(path);
    auto ms = make_test_ms(path);

    auto stats = ms_flag_stats(ms);
    (void)stats;
    assert(stats.total_rows == 5);
    assert(stats.flagged_rows == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_flag_rows() {
    std::cout << "  flag specific rows... ";

    auto path = make_temp_dir("flagrows");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Flag rows 1 and 3.
    ms_flag_rows(ms, {1, 3});

    // Re-open to read fresh data.
    auto ms2 = MeasurementSet::open(path);
    auto stats = ms_flag_stats(ms2);
    (void)stats;
    assert(stats.total_rows == 5);
    assert(stats.flagged_rows == 2);

    // Verify specific rows.
    MsMainColumns cols(ms2);
    assert(!cols.flag_row(0));
    assert(cols.flag_row(1));
    assert(!cols.flag_row(2));
    assert(cols.flag_row(3));
    assert(!cols.flag_row(4));

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_unflag_rows() {
    std::cout << "  unflag specific rows... ";

    auto path = make_temp_dir("unflag");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Flag all rows.
    ms_flag_rows(ms, {0, 1, 2, 3, 4});

    {
        auto ms2 = MeasurementSet::open(path);
        assert(ms_flag_stats(ms2).flagged_rows == 5);
    }

    // Unflag rows 0, 2, 4.
    auto ms3 = MeasurementSet::open(path);
    ms_unflag_rows(ms3, {0, 2, 4});

    auto ms4 = MeasurementSet::open(path);
    auto stats = ms_flag_stats(ms4);
    (void)stats;
    assert(stats.flagged_rows == 2);

    MsMainColumns cols(ms4);
    assert(!cols.flag_row(0));
    assert(cols.flag_row(1));
    assert(!cols.flag_row(2));
    assert(cols.flag_row(3));
    assert(!cols.flag_row(4));

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_flag_preserves_other_data() {
    std::cout << "  flagging preserves other column data... ";

    auto path = make_temp_dir("preserve");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Read original values.
    MsMainColumns cols_before(ms);
    auto ant1_0 = cols_before.antenna1(0);
    auto time_0 = cols_before.time(0);
    auto scan_0 = cols_before.scan_number(0);
    (void)ant1_0;
    (void)time_0;
    (void)scan_0;

    // Flag row 0.
    ms_flag_rows(ms, {0});

    // Re-open and check other columns.
    auto ms2 = MeasurementSet::open(path);
    MsMainColumns cols_after(ms2);
    assert(cols_after.antenna1(0) == ant1_0);
    assert(cols_after.time(0) == time_0);
    assert(cols_after.scan_number(0) == scan_0);
    assert(cols_after.flag_row(0));

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_empty_flag_operations() {
    std::cout << "  empty flag operations (no-ops)... ";

    auto path = make_temp_dir("empty");
    cleanup(path);
    auto ms = make_test_ms(path);

    // Flag/unflag with empty row list.
    ms_flag_rows(ms, {});
    ms_unflag_rows(ms, {});

    auto ms2 = MeasurementSet::open(path);
    assert(ms_flag_stats(ms2).flagged_rows == 0);

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_flagger_test\n";

        test_initial_flag_stats();
        test_flag_rows();
        test_unflag_rows();
        test_flag_preserves_other_data();
        test_empty_flag_operations();

        std::cout << "all ms_flagger tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
