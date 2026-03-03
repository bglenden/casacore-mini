// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_summary.hpp"
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
               ("ms_sum_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void test_summary_contains_required_sections() {
    std::cout << "  summary contains required sections... ";

    auto path = make_temp_dir("sections");
    cleanup(path);

    {
        auto ms = MeasurementSet::create(path, false);
        MsWriter writer(ms);

        writer.add_antenna({.name = "ANT0",
                            .station = "PAD0",
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0});
        writer.add_antenna({.name = "ANT1",
                            .station = "PAD1",
                            .type = "GROUND-BASED",
                            .mount = "ALT-AZ",
                            .position = {},
                            .offset = {},
                            .dish_diameter = 12.0});
        writer.add_field({.name = "3C273", .code = {}});
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
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
        writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
        writer.add_observation({.telescope_name = "ALMA",
                                .observer = "Jones",
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
                        .scan_number = 1,
                        .state_id = 0,
                        .time = 4.8e9,
                        .time_centroid = 4.8e9,
                        .uvw = {100.0, 200.0, 50.0},
                        .sigma = {1.0F, 1.0F},
                        .weight = {1.0F, 1.0F},
                        .data = {},
                        .flag = {}});
        writer.flush();
    }

    auto ms = MeasurementSet::open(path);
    auto summary = ms_summary(ms);

    // Check required sections are present.
    assert(summary.find("MeasurementSet:") != std::string::npos);
    assert(summary.find("Rows: 1") != std::string::npos);
    assert(summary.find("Observation:") != std::string::npos);
    assert(summary.find("ALMA") != std::string::npos);
    assert(summary.find("Jones") != std::string::npos);
    assert(summary.find("Antennas: 2") != std::string::npos);
    assert(summary.find("ANT0") != std::string::npos);
    assert(summary.find("ANT1") != std::string::npos);
    assert(summary.find("Fields: 1") != std::string::npos);
    assert(summary.find("3C273") != std::string::npos);
    assert(summary.find("Spectral Windows: 1") != std::string::npos);
    assert(summary.find("L-band") != std::string::npos);
    assert(summary.find("1400") != std::string::npos); // 1.4e9 / 1e6 = 1400 MHz
    assert(summary.find("nchan=64") != std::string::npos);
    assert(summary.find("Scans: 1") != std::string::npos);
    assert(summary.find("Subtables:") != std::string::npos);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_summary_empty_ms() {
    std::cout << "  summary on empty MS... ";

    auto path = make_temp_dir("empty");
    cleanup(path);

    auto ms = MeasurementSet::create(path, false);
    auto ms2 = MeasurementSet::open(path);
    auto summary = ms_summary(ms2);

    assert(summary.find("Rows: 0") != std::string::npos);
    assert(summary.find("Antennas: 0") != std::string::npos);
    assert(summary.find("Fields: 0") != std::string::npos);
    assert(summary.find("Scans: 0") != std::string::npos);

    cleanup(path);
    std::cout << "PASS\n";
}

static void test_summary_multi_field_spw() {
    std::cout << "  summary with multiple fields and SPWs... ";

    auto path = make_temp_dir("multifld");
    cleanup(path);

    {
        auto ms = MeasurementSet::create(path, false);
        MsWriter writer(ms);

        for (int i = 0; i < 4; ++i) {
            writer.add_antenna({.name = "A" + std::to_string(i),
                                .station = "P" + std::to_string(i),
                                .type = "GROUND-BASED",
                                .mount = "ALT-AZ",
                                .position = {},
                                .offset = {},
                                .dish_diameter = 25.0});
        }
        writer.add_field({.name = "SRC_A", .code = {}});
        writer.add_field({.name = "SRC_B", .code = {}});
        writer.add_field({.name = "SRC_C", .code = {}});
        writer.add_spectral_window({.num_chan = 32,
                                    .name = "Band3",
                                    .ref_frequency = 100e9,
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
        writer.add_spectral_window({.num_chan = 64,
                                    .name = "Band6",
                                    .ref_frequency = 230e9,
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
        writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0});
        writer.add_polarization({.num_corr = 4, .corr_type = {}});
        writer.add_observation({.telescope_name = "ALMA", .observer = "test", .project = {}});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "OBSERVE"});

        for (int r = 0; r < 5; ++r) {
            writer.add_row({.antenna1 = 0,
                            .antenna2 = 1,
                            .array_id = 0,
                            .data_desc_id = 0,
                            .exposure = 0.0,
                            .feed1 = 0,
                            .feed2 = 0,
                            .field_id = r % 3,
                            .flag_row = false,
                            .interval = 0.0,
                            .observation_id = 0,
                            .processor_id = 0,
                            .scan_number = r + 1,
                            .state_id = 0,
                            .time = 4.8e9 + static_cast<double>(r) * 10.0,
                            .time_centroid = 4.8e9 + static_cast<double>(r) * 10.0,
                            .uvw = {100.0, 200.0, 50.0},
                            .sigma = {1.0F, 1.0F, 1.0F, 1.0F},
                            .weight = {1.0F, 1.0F, 1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }
        writer.flush();
    }

    auto ms = MeasurementSet::open(path);
    auto summary = ms_summary(ms);

    assert(summary.find("Antennas: 4") != std::string::npos);
    assert(summary.find("Fields: 3") != std::string::npos);
    assert(summary.find("Spectral Windows: 2") != std::string::npos);
    assert(summary.find("SRC_A") != std::string::npos);
    assert(summary.find("SRC_B") != std::string::npos);
    assert(summary.find("SRC_C") != std::string::npos);
    assert(summary.find("Band3") != std::string::npos);
    assert(summary.find("Band6") != std::string::npos);
    assert(summary.find("Scans: 5") != std::string::npos);

    cleanup(path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_summary_test\n";

        test_summary_contains_required_sections();
        test_summary_empty_ms();
        test_summary_multi_field_spw();

        std::cout << "all ms_summary tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
