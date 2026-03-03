// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_metadata.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_meta_" + suffix + "_" +
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
                        .dish_diameter = 12.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT2",
                        .station = "PAD2",
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
    writer.add_field({.name = "M31",
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
    writer.add_spectral_window({.num_chan = 128,
                                .name = "S-band",
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

    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "Smith",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE",
                      .flag_row = false});

    // 6 rows: scan 1 (3 rows), scan 2 (3 rows).
    for (int scan = 1; scan <= 2; ++scan) {
        for (int bl = 0; bl < 3; ++bl) {
            writer.add_row({.antenna1 = bl,
                            .antenna2 = (bl + 1) % 3,
                            .array_id = 0,
                            .data_desc_id = 0,
                            .exposure = 0.0,
                            .feed1 = 0,
                            .feed2 = 0,
                            .field_id = scan - 1,
                            .flag_row = false,
                            .interval = 0.0,
                            .observation_id = 0,
                            .processor_id = 0,
                            .scan_number = scan,
                            .state_id = 0,
                            .time = 4.8e9 + static_cast<double>(scan * 100 + bl),
                            .time_centroid = 4.8e9 + static_cast<double>(scan * 100 + bl),
                            .uvw = {100.0, 200.0, 50.0},
                            .sigma = {1.0F, 1.0F},
                            .weight = {1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }
    }
    writer.flush();
    return MeasurementSet::open(path);
}

static void test_antenna_metadata() {
    std::cout << "  antenna metadata... ";

    auto path = make_temp_dir("ant");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    assert(md.n_antennas() == 3);
    assert(md.antenna_names()[0] == "ANT0");
    assert(md.antenna_names()[2] == "ANT2");
    assert(md.antenna_stations()[1] == "PAD1");
    assert(std::abs(md.antenna_diameters()[0] - 25.0) < 1e-10);
    assert(std::abs(md.antenna_diameters()[1] - 12.0) < 1e-10);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_field_metadata() {
    std::cout << "  field metadata... ";

    auto path = make_temp_dir("fld");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    assert(md.n_fields() == 2);
    assert(md.field_names()[0] == "3C273");
    assert(md.field_names()[1] == "M31");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_spw_metadata() {
    std::cout << "  spectral window metadata... ";

    auto path = make_temp_dir("spw");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    assert(md.n_spws() == 2);
    assert(md.spw_names()[0] == "L-band");
    assert(md.spw_names()[1] == "S-band");
    assert(std::abs(md.spw_ref_frequencies()[0] - 1.4e9) < 1.0);
    assert(std::abs(md.spw_ref_frequencies()[1] - 3.0e9) < 1.0);
    assert(md.spw_num_channels()[0] == 64);
    assert(md.spw_num_channels()[1] == 128);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_observation_metadata() {
    std::cout << "  observation metadata... ";

    auto path = make_temp_dir("obs");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    assert(md.n_observations() == 1);
    assert(md.telescope_names()[0] == "VLA");
    assert(md.observers()[0] == "Smith");

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_main_table_aggregates() {
    std::cout << "  main table aggregates... ";

    auto path = make_temp_dir("main");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    assert(md.n_rows() == 6);
    assert(md.scan_numbers().size() == 2);
    assert(md.scan_numbers().count(1) > 0);
    assert(md.scan_numbers().count(2) > 0);
    assert(md.array_ids().size() == 1);
    assert(md.observation_ids().size() == 1);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_empty_ms() {
    std::cout << "  metadata on empty MS... ";

    auto path = make_temp_dir("empty");
    cleanup(path);
    auto ms = MeasurementSet::create(path, false);

    // Empty MS with only schema, no data rows or subtable rows.
    auto ms2 = MeasurementSet::open(path);
    MsMetaData md(ms2);

    assert(md.n_rows() == 0);
    assert(md.n_antennas() == 0);
    assert(md.n_fields() == 0);
    assert(md.scan_numbers().empty());

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_cache_consistency() {
    std::cout << "  cache consistency (repeated calls)... ";

    auto path = make_temp_dir("cache");
    cleanup(path);
    auto ms = make_test_ms(path);
    MsMetaData md(ms);

    // Call twice to verify caching works.
    auto& names1 = md.antenna_names();
    auto& names2 = md.antenna_names();
    (void)names1;
    (void)names2;
    assert(&names1 == &names2); // Same reference, not re-computed.
    assert(names1.size() == 3);

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_metadata_test\n";

        test_antenna_metadata();
        test_field_metadata();
        test_spw_metadata();
        test_observation_metadata();
        test_main_table_aggregates();
        test_empty_ms();
        test_cache_consistency();

        std::cout << "all ms_metadata tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
