// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_columns.hpp"
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
               ("ms_wr_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void test_write_and_reread() {
    std::cout << "  write + reread round-trip... ";

    auto ms_path = make_temp_dir("roundtrip");
    cleanup(ms_path);

    {
        auto ms = MeasurementSet::create(ms_path, true);
        MsWriter writer(ms);

        // Subtable rows (FK targets).
        writer.add_antenna({.name = "ANT0",
                            .station = "PAD0",
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {-1601185.0, -5041978.0, 3554876.0},
                            .offset = {0.0, 0.0, 0.0},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_antenna({.name = "ANT1",
                            .station = "PAD1",
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {-1601192.0, -5041981.0, 3554871.0},
                            .offset = {0.0, 0.0, 0.0},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_spectral_window({.num_chan = 64,
                                    .name = "SPW0",
                                    .ref_frequency = 1.4e9,
                                    .chan_freq = {},
                                    .chan_width = {},
                                    .effective_bw = {},
                                    .resolution = {},
                                    .meas_freq_ref = 5,
                                    .total_bandwidth = 2.0e6,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = {},
                                    .flag_row = false});
        writer.add_field({.name = "3C273",
                          .code = "T",
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
        writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false}); // RR, LL
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
                          .obs_mode = "OBSERVE_TARGET.ON_SOURCE",
                          .flag_row = false});

        // Main-table rows.
        writer.add_row({.antenna1 = 0,
                        .antenna2 = 1,
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
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
        writer.add_row({.antenna1 = 1,
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
                        .time = 4.8e9 + 10.0,
                        .time_centroid = 4.8e9 + 10.0,
                        .uvw = {-100.0, -200.0, -50.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});

        writer.flush();
    }

    // Reopen and verify.
    {
        auto ms = MeasurementSet::open(ms_path);
        assert(ms.row_count() == 2);

        // Verify main table scalars.
        MsMainColumns cols(ms);
        assert(cols.antenna1(0) == 0);
        assert(cols.antenna2(0) == 1);
        assert(cols.antenna1(1) == 1);
        assert(cols.antenna2(1) == 0);
        assert(cols.data_desc_id(0) == 0);
        assert(cols.field_id(0) == 0);
        assert(cols.scan_number(0) == 1);
        assert(std::abs(cols.exposure(0) - 10.0) < 1e-10);
        assert(std::abs(cols.time(0) - 4.8e9) < 1.0);
        assert(std::abs(cols.time(1) - (4.8e9 + 10.0)) < 1.0);

        // Verify main table arrays (UVW).
        auto uvw0 = cols.uvw(0);
        assert(uvw0.size() == 3);
        assert(std::abs(uvw0[0] - 100.0) < 1e-10);
        assert(std::abs(uvw0[1] - 200.0) < 1e-10);
        assert(std::abs(uvw0[2] - 50.0) < 1e-10);

        auto uvw1 = cols.uvw(1);
        assert(std::abs(uvw1[0] - (-100.0)) < 1e-10);

        // Verify ANTENNA subtable.
        MsAntennaColumns ant(ms);
        assert(ant.row_count() == 2);
        assert(ant.name(0) == "ANT0");
        assert(ant.name(1) == "ANT1");
        assert(ant.station(0) == "PAD0");
        assert(std::abs(ant.dish_diameter(0) - 25.0) < 1e-10);

        // Verify SPECTRAL_WINDOW subtable.
        MsSpWindowColumns spw(ms);
        assert(spw.row_count() == 1);
        assert(spw.num_chan(0) == 64);
        assert(spw.name(0) == "SPW0");
        assert(std::abs(spw.ref_frequency(0) - 1.4e9) < 1.0);

        // Verify FIELD subtable.
        MsFieldColumns fld(ms);
        assert(fld.row_count() == 1);
        assert(fld.name(0) == "3C273");
        assert(fld.code(0) == "T");

        // Verify DATA_DESCRIPTION subtable.
        MsDataDescColumns dd(ms);
        assert(dd.row_count() == 1);
        assert(dd.spectral_window_id(0) == 0);
        assert(dd.polarization_id(0) == 0);

        // Verify OBSERVATION subtable.
        MsObservationColumns obs(ms);
        assert(obs.row_count() == 1);
        assert(obs.telescope_name(0) == "VLA");
        assert(obs.observer(0) == "test");
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_fk_validation_catches_errors() {
    std::cout << "  FK validation catches invalid refs... ";

    auto ms_path = make_temp_dir("fkcheck");
    cleanup(ms_path);

    auto ms = MeasurementSet::create(ms_path, false);
    MsWriter writer(ms);

    // Add some subtable rows, but leave gaps.
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
    writer.add_observation({.telescope_name = "VLA",
                            .observer = {},
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
    // No data_description added -> dd_rows is empty.

    // Add a main row with data_desc_id=0 (no DD rows exist).
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
                    .scan_number = 0,
                    .state_id = 0,
                    .time = 4.8e9,
                    .time_centroid = 0.0,
                    .uvw = {0.0, 0.0, 0.0},
                    .sigma = {1.0F},
                    .weight = {1.0F},
                    .data = {},
                    .flag = {}});

    bool caught = false;
    try {
        writer.validate_foreign_keys();
    } catch (const std::runtime_error& e) {
        caught = true;
        std::string msg = e.what();
        // Should mention DATA_DESC_ID.
        assert(msg.find("DATA_DESC_ID") != std::string::npos);
    }
    (void)caught;
    assert(caught);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_fk_validation_passes_valid() {
    std::cout << "  FK validation passes valid data... ";

    auto ms_path = make_temp_dir("fkvalid");
    cleanup(ms_path);

    auto ms = MeasurementSet::create(ms_path, false);
    MsWriter writer(ms);

    writer.add_antenna({.name = "ANT0",
                        .station = {},
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 0.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT1",
                        .station = {},
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 0.0,
                        .flag_row = false});
    writer.add_field(
        {.name = "F0", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 1, .corr_type = {}, .flag_row = false});
    writer.add_spectral_window({.num_chan = 1,
                                .name = "S0",
                                .ref_frequency = 0.0,
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
    writer.add_observation({.telescope_name = "VLA",
                            .observer = {},
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
                    .scan_number = 0,
                    .state_id = 0,
                    .time = 4.8e9,
                    .time_centroid = 0.0,
                    .uvw = {0.0, 0.0, 0.0},
                    .sigma = {1.0F},
                    .weight = {1.0F},
                    .data = {},
                    .flag = {}});

    // Should not throw.
    writer.validate_foreign_keys();

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_empty_write() {
    std::cout << "  flush with no rows is a no-op... ";

    auto ms_path = make_temp_dir("emptywr");
    cleanup(ms_path);

    auto ms = MeasurementSet::create(ms_path, false);
    MsWriter writer(ms);
    // No rows added — flush should succeed without errors.
    writer.flush();

    auto ms2 = MeasurementSet::open(ms_path);
    assert(ms2.row_count() == 0);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_multiple_subtable_rows() {
    std::cout << "  multiple subtable rows... ";

    auto ms_path = make_temp_dir("multi");
    cleanup(ms_path);

    {
        auto ms = MeasurementSet::create(ms_path, false);
        MsWriter writer(ms);

        // 4 antennas.
        for (int i = 0; i < 4; ++i) {
            writer.add_antenna({.name = "ANT" + std::to_string(i),
                                .station = "PAD" + std::to_string(i),
                                .type = "GROUND-BASED",
                                .mount = "ALT-AZ",
                                .position = {},
                                .offset = {},
                                .dish_diameter = 12.0 + static_cast<double>(i),
                                .flag_row = false});
        }
        writer.add_field({.name = "SRC_A",
                          .code = {},
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_field({.name = "SRC_B",
                          .code = {},
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_data_description({});
        writer.add_polarization({.num_corr = 4, .corr_type = {}, .flag_row = false});
        writer.add_spectral_window({.num_chan = 128,
                                    .name = "Band3",
                                    .ref_frequency = 0.0,
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
        writer.add_observation({.telescope_name = "ALMA",
                                .observer = {},
                                .project = {},
                                .release_date = 0.0,
                                .flag_row = false});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "CALIBRATE",
                          .flag_row = false});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "OBSERVE",
                          .flag_row = false});

        // 3 baselines.
        for (int a1 = 0; a1 < 3; ++a1) {
            writer.add_row({.antenna1 = a1,
                            .antenna2 = a1 + 1,
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
                            .state_id = 1,
                            .time = 5.0e9 + static_cast<double>(a1) * 10.0,
                            .time_centroid = 5.0e9 + static_cast<double>(a1) * 10.0,
                            .uvw = {static_cast<double>(a1), 0.0, 0.0},
                            .sigma = {1.0F, 1.0F, 1.0F},
                            .weight = {1.0F, 1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }

        writer.flush();
    }

    // Reopen and verify counts.
    {
        auto ms = MeasurementSet::open(ms_path);
        assert(ms.row_count() == 3);

        MsAntennaColumns ant(ms);
        assert(ant.row_count() == 4);
        assert(ant.name(3) == "ANT3");

        MsFieldColumns fld(ms);
        assert(fld.row_count() == 2);
        assert(fld.name(1) == "SRC_B");
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_writer_test\n";

        test_write_and_reread();
        test_fk_validation_catches_errors();
        test_fk_validation_passes_valid();
        test_empty_write();
        test_multiple_subtable_rows();

        std::cout << "all ms_writer tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
