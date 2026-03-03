// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_enums.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void test_ms_enums() {
    std::cout << "  ms_enums: column info lookup... ";

    // Check required columns.
    const auto& time_info = casacore_mini::ms_main_column_info(casacore_mini::MsMainColumn::time);
    (void)time_info;
    assert(time_info.name == "TIME");
    assert(time_info.data_type == casacore_mini::DataType::tp_double);
    assert(time_info.kind == casacore_mini::ColumnKind::scalar);
    assert(time_info.required);

    const auto& uvw_info = casacore_mini::ms_main_column_info(casacore_mini::MsMainColumn::uvw);
    (void)uvw_info;
    assert(uvw_info.name == "UVW");
    assert(uvw_info.data_type == casacore_mini::DataType::tp_double);
    assert(uvw_info.kind == casacore_mini::ColumnKind::array);
    assert(uvw_info.ndim == 1);
    assert(uvw_info.shape.size() == 1);
    assert(uvw_info.shape[0] == 3);
    assert(uvw_info.required);

    const auto& data_info = casacore_mini::ms_main_column_info(casacore_mini::MsMainColumn::data);
    (void)data_info;
    assert(data_info.name == "DATA");
    assert(!data_info.required);

    // Name lookup.
    auto ant1 = casacore_mini::ms_main_column_from_name("ANTENNA1");
    (void)ant1;
    assert(ant1.has_value());
    assert(*ant1 == casacore_mini::MsMainColumn::antenna1);

    auto unknown = casacore_mini::ms_main_column_from_name("NONEXISTENT");
    (void)unknown;
    assert(!unknown.has_value());

    // Required columns count.
    auto required = casacore_mini::ms_required_main_columns();
    assert(required.size() == 21);

    // Subtable names.
    auto req_subs = casacore_mini::ms_required_subtable_names();
    assert(req_subs.size() == 12);

    auto all_subs = casacore_mini::ms_all_subtable_names();
    assert(all_subs.size() == 17);

    // Keyword names.
    auto kw_name = casacore_mini::ms_main_keyword_name(casacore_mini::MsMainKeyword::antenna);
    (void)kw_name;
    assert(kw_name == "ANTENNA");
    assert(casacore_mini::ms_main_keyword_required(casacore_mini::MsMainKeyword::antenna));
    assert(!casacore_mini::ms_main_keyword_required(casacore_mini::MsMainKeyword::doppler));

    std::cout << "PASS\n";
}

static void test_create_and_reopen() {
    std::cout << "  create + reopen round-trip... ";

    auto ms_path = make_temp_dir("create");
    cleanup(ms_path);

    {
        auto ms = casacore_mini::MeasurementSet::create(ms_path, true);
        assert(ms.row_count() == 0);
        assert(ms.path() == ms_path);

        // All 12 required subtables must exist.
        auto req_subs = casacore_mini::ms_required_subtable_names();
        for (const auto& name : req_subs) {
            (void)name;
            assert(ms.has_subtable(name));
        }

        // Main table columns include required ones.
        assert(ms.main_table().columns().size() >= 21);

        // table.info type.
        auto info_type = casacore_mini::read_table_info_type(ms_path);
        assert(info_type == "Measurement Set");
    }

    // Reopen and verify.
    {
        auto ms = casacore_mini::MeasurementSet::open(ms_path);
        assert(ms.row_count() == 0);

        // Subtables still present.
        assert(ms.has_subtable("ANTENNA"));
        assert(ms.has_subtable("SPECTRAL_WINDOW"));
        assert(ms.has_subtable("POLARIZATION"));

        // Can read subtable.
        const auto& ant = ms.subtable("ANTENNA");
        (void)ant;
        assert(ant.nrow() == 0);
        assert(!ant.columns().empty());
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_subtable_schemas() {
    std::cout << "  subtable schema correctness... ";

    auto ms_path = make_temp_dir("schemas");
    cleanup(ms_path);

    auto ms = casacore_mini::MeasurementSet::create(ms_path, true);

    // Verify ANTENNA subtable has expected columns.
    {
        const auto& ant = ms.subtable("ANTENNA");
        const auto& cols = ant.columns();
        bool has_name = false;
        bool has_position = false;
        bool has_dish_diameter = false;
        for (const auto& col : cols) {
            if (col.name == "NAME") {
                has_name = true;
            }
            if (col.name == "POSITION") {
                has_position = true;
            }
            if (col.name == "DISH_DIAMETER") {
                has_dish_diameter = true;
            }
        }
        (void)has_name;
        (void)has_position;
        (void)has_dish_diameter;
        assert(has_name);
        assert(has_position);
        assert(has_dish_diameter);
    }

    // Verify SPECTRAL_WINDOW subtable.
    {
        const auto& spw = ms.subtable("SPECTRAL_WINDOW");
        const auto& cols = spw.columns();
        bool has_num_chan = false;
        bool has_chan_freq = false;
        bool has_ref_freq = false;
        for (const auto& col : cols) {
            if (col.name == "NUM_CHAN") {
                has_num_chan = true;
            }
            if (col.name == "CHAN_FREQ") {
                has_chan_freq = true;
            }
            if (col.name == "REF_FREQUENCY") {
                has_ref_freq = true;
            }
        }
        (void)has_num_chan;
        (void)has_chan_freq;
        (void)has_ref_freq;
        assert(has_num_chan);
        assert(has_chan_freq);
        assert(has_ref_freq);
    }

    // Verify POLARIZATION subtable.
    {
        const auto& pol = ms.subtable("POLARIZATION");
        const auto& cols = pol.columns();
        bool has_num_corr = false;
        bool has_corr_type = false;
        for (const auto& col : cols) {
            if (col.name == "NUM_CORR") {
                has_num_corr = true;
            }
            if (col.name == "CORR_TYPE") {
                has_corr_type = true;
            }
        }
        (void)has_num_corr;
        (void)has_corr_type;
        assert(has_num_corr);
        assert(has_corr_type);
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_subtable_names_list() {
    std::cout << "  subtable_names() listing... ";

    auto ms_path = make_temp_dir("names");
    cleanup(ms_path);

    auto ms = casacore_mini::MeasurementSet::create(ms_path, false);
    auto names = ms.subtable_names();

    // Should have exactly the 12 required subtables.
    assert(names.size() == 12);

    // Check a few specific names.
    bool found_antenna = false;
    bool found_spw = false;
    bool found_state = false;
    for (const auto& n : names) {
        if (n == "ANTENNA") {
            found_antenna = true;
        }
        if (n == "SPECTRAL_WINDOW") {
            found_spw = true;
        }
        if (n == "STATE") {
            found_state = true;
        }
    }
    (void)found_antenna;
    (void)found_spw;
    (void)found_state;
    assert(found_antenna);
    assert(found_spw);
    assert(found_state);

    // Optional subtables should not exist.
    assert(!ms.has_subtable("DOPPLER"));
    assert(!ms.has_subtable("WEATHER"));
    assert(!ms.has_subtable("SOURCE"));

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_measure_keywords() {
    std::cout << "  measure keywords on TIME/UVW... ";

    auto ms_path = make_temp_dir("measinfo");
    cleanup(ms_path);

    auto ms = casacore_mini::MeasurementSet::create(ms_path, true);
    const auto& cols = ms.main_table().columns();

    // Find TIME column and check MEASINFO.
    for (const auto& col : cols) {
        if (col.name == "TIME") {
            const auto* measinfo = col.keywords.find("MEASINFO");
            (void)measinfo;
            assert(measinfo != nullptr);
            const auto* units = col.keywords.find("QuantumUnits");
            (void)units;
            assert(units != nullptr);
        }
        if (col.name == "UVW") {
            const auto* measinfo = col.keywords.find("MEASINFO");
            (void)measinfo;
            assert(measinfo != nullptr);
            const auto* units = col.keywords.find("QuantumUnits");
            (void)units;
            assert(units != nullptr);
        }
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_malformed_paths() {
    std::cout << "  malformed path handling... ";

    // Open non-existent path.
    bool caught = false;
    try {
        (void)casacore_mini::MeasurementSet::open("/nonexistent/ms");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    // Create at existing path.
    auto ms_path = make_temp_dir("exists");
    cleanup(ms_path);
    (void)casacore_mini::MeasurementSet::create(ms_path, false);

    caught = false;
    try {
        (void)casacore_mini::MeasurementSet::create(ms_path, false);
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(ms_path);

    // Access non-existent subtable.
    auto ms_path2 = make_temp_dir("nosub");
    cleanup(ms_path2);
    auto ms = casacore_mini::MeasurementSet::create(ms_path2, false);
    caught = false;
    try {
        (void)ms.subtable("NONEXISTENT");
    } catch (const std::runtime_error&) {
        caught = true;
    }
    (void)caught;
    assert(caught);

    cleanup(ms_path2);
    std::cout << "PASS\n";
}

static void test_flush_round_trip() {
    std::cout << "  flush + reopen preserves state... ";

    auto ms_path = make_temp_dir("flush");
    cleanup(ms_path);

    {
        auto ms = casacore_mini::MeasurementSet::create(ms_path, true);

        // Add real rows via MsWriter to test flush round-trip.
        casacore_mini::MsWriter writer(ms);
        writer.add_antenna({.name = "A0",
                            .station = "S0",
                            .position = {0, 0, 0},
                            .offset = {0, 0, 0},
                            .dish_diameter = 25.0});
        writer.add_antenna({.name = "A1",
                            .station = "S1",
                            .position = {1, 0, 0},
                            .offset = {0, 0, 0},
                            .dish_diameter = 25.0});
        writer.add_spectral_window({.num_chan = 1,
                                    .name = "SPW0",
                                    .ref_frequency = 1e9,
                                    .chan_freq = {1e9},
                                    .chan_width = {1e6},
                                    .effective_bw = {1e6},
                                    .resolution = {1e6},
                                    .meas_freq_ref = 0,
                                    .total_bandwidth = 1e6,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = "",
                                    .flag_row = false});
        writer.add_polarization({.num_corr = 1, .corr_type = {5}});
        writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0});
        writer.add_observation({.telescope_name = "TEST",
                                .observer = "",
                                .project = "",
                                .release_date = 0.0,
                                .flag_row = false});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "OBSERVE",
                          .flag_row = false});
        writer.add_field({.name = "SRC",
                          .code = "",
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});

        for (int i = 0; i < 3; ++i) {
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
                            .time_centroid = 4.8e9,
                            .uvw = {1, 2, 3},
                            .sigma = {1.0F},
                            .weight = {1.0F},
                            .data = {},
                            .flag = {}});
        }
        writer.flush();
    }

    {
        auto ms = casacore_mini::MeasurementSet::open(ms_path);
        assert(ms.row_count() == 3);
    }

    cleanup(ms_path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "measurement_set_test\n";

        test_ms_enums();
        test_create_and_reopen();
        test_subtable_schemas();
        test_subtable_names_list();
        test_measure_keywords();
        test_malformed_paths();
        test_flush_round_trip();

        std::cout << "all measurement_set tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
