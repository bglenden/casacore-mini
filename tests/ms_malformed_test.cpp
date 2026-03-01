#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_metadata.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_malf_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void test_open_nonexistent_path() {
    std::cout << "  open non-existent path throws... ";

    bool caught = false;
    try {
        (void)MeasurementSet::open("/does/not/exist/ms.ms");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    std::cout << "PASS\n";
}

static void test_open_empty_directory() {
    std::cout << "  open empty directory throws... ";

    auto path = make_temp_dir("emptydir");
    cleanup(path);
    fs::create_directories(path);

    bool caught = false;
    try {
        (void)MeasurementSet::open(path);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_open_truncated_table_dat() {
    std::cout << "  open with truncated table.dat throws... ";

    auto path = make_temp_dir("truncated");
    cleanup(path);
    fs::create_directories(path);

    // Write a truncated table.dat.
    {
        std::ofstream out(path / "table.dat", std::ios::binary);
        // Write just a few garbage bytes.
        const char garbage[] = {'\x00', '\x01', '\x02', '\x03'};
        out.write(garbage, sizeof(garbage));
    }

    bool caught = false;
    try {
        (void)MeasurementSet::open(path);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_create_over_existing_fails() {
    std::cout << "  create over existing path throws... ";

    auto path = make_temp_dir("existing");
    cleanup(path);

    // First creation succeeds.
    (void)MeasurementSet::create(path, false);

    // Second creation should fail.
    bool caught = false;
    try {
        (void)MeasurementSet::create(path, false);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_subtable_missing() {
    std::cout << "  access missing subtable throws... ";

    auto path = make_temp_dir("missingsub");
    cleanup(path);

    auto ms = MeasurementSet::create(path, false);

    // Remove a required subtable directory.
    fs::remove_all(path / "ANTENNA");

    auto ms2 = MeasurementSet::open(path);
    bool caught = false;
    try {
        (void)ms2.subtable("ANTENNA");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_metadata_on_valid_ms() {
    std::cout << "  metadata on valid MS succeeds... ";

    auto path = make_temp_dir("validmeta");
    cleanup(path);

    {
        auto ms = MeasurementSet::create(path, false);
        MsWriter writer(ms);
        writer.add_antenna({.name = "ANT0",
                            .station = {},
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_field({.name = "F0",
                          .code = {},
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_spectral_window({.num_chan = 64,
                                    .name = "S0",
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
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
        writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
        writer.add_observation({.telescope_name = "VLA",
                                .observer = {},
                                .project = {},
                                .release_date = 0.0,
                                .flag_row = false});
        writer.add_state({.obs_mode = "OBSERVE"});
        writer.add_row({.antenna1 = 0,
                        .antenna2 = 0,
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
                        .time = 4.8e9,
                        .time_centroid = 4.8e9,
                        .uvw = {0.0, 0.0, 0.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
        writer.flush();
    }

    auto ms = MeasurementSet::open(path);
    MsMetaData md(ms);
    assert(md.n_antennas() == 1);
    assert(md.n_fields() == 1);
    assert(md.n_rows() == 1);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_selection_on_empty_subtable() {
    std::cout << "  selection with empty subtable works... ";

    auto path = make_temp_dir("emptysel");
    cleanup(path);

    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);
    // Only add required subtable rows to avoid FK errors,
    // but no main-table rows.
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
        std::cout << "ms_malformed_test\n";

        test_open_nonexistent_path();
        test_open_empty_directory();
        test_open_truncated_table_dat();
        test_create_over_existing_fails();
        test_subtable_missing();
        test_metadata_on_valid_ms();
        test_selection_on_empty_subtable();

        std::cout << "all ms_malformed tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
