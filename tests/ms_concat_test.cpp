#include "casacore_mini/ms_concat.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/ms_metadata.hpp"
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
               ("ms_cat_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

static void populate_ms1(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    writer.add_antenna({.name = "ANT0", .station = "PAD0", .type = "GROUND-BASED",
                         .mount = "ALT-AZ", .position = {}, .offset = {},
                         .dish_diameter = 25.0});
    writer.add_antenna({.name = "ANT1", .station = "PAD1", .type = "GROUND-BASED",
                         .mount = "ALT-AZ", .position = {}, .offset = {},
                         .dish_diameter = 25.0});
    writer.add_field({.name = "3C273", .code = {}});
    writer.add_spectral_window({.num_chan = 64, .name = "L-band", .ref_frequency = 1.4e9,
                                 .chan_freq = {}, .chan_width = {}, .effective_bw = {},
                                 .resolution = {}, .meas_freq_ref = 0,
                                 .total_bandwidth = 0.0, .net_sideband = 0,
                                 .if_conv_chain = 0, .freq_group = 0,
                                 .freq_group_name = {}, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0,
                                  .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA", .observer = "test", .project = {},
                             .release_date = 0.0, .flag_row = false});
    writer.add_state({.obs_mode = "OBSERVE"});

    writer.add_row({.antenna1 = 0, .antenna2 = 1, .array_id = 0,
                     .data_desc_id = 0, .exposure = 0.0, .feed1 = 0, .feed2 = 0,
                     .field_id = 0, .flag_row = false, .interval = 0.0,
                     .observation_id = 0, .processor_id = 0,
                     .scan_number = 1, .state_id = 0, .time = 4.8e9,
                     .time_centroid = 4.8e9,
                     .uvw = {100.0, 200.0, 50.0},
                     .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                     .data = {}, .flag = {}});
    writer.add_row({.antenna1 = 1, .antenna2 = 0, .array_id = 0,
                     .data_desc_id = 0, .exposure = 0.0, .feed1 = 0, .feed2 = 0,
                     .field_id = 0, .flag_row = false, .interval = 0.0,
                     .observation_id = 0, .processor_id = 0,
                     .scan_number = 1, .state_id = 0, .time = 4.8e9 + 10.0,
                     .time_centroid = 4.8e9 + 10.0,
                     .uvw = {-100.0, -200.0, -50.0},
                     .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                     .data = {}, .flag = {}});
    writer.flush();
}

static void populate_ms2(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    // ANT0 is shared, ANT2 is new.
    writer.add_antenna({.name = "ANT0", .station = "PAD0", .type = "GROUND-BASED",
                         .mount = "ALT-AZ", .position = {}, .offset = {},
                         .dish_diameter = 25.0});
    writer.add_antenna({.name = "ANT2", .station = "PAD2", .type = "GROUND-BASED",
                         .mount = "ALT-AZ", .position = {}, .offset = {},
                         .dish_diameter = 12.0});
    // 3C273 is shared, M31 is new.
    writer.add_field({.name = "3C273", .code = {}});
    writer.add_field({.name = "M31", .code = {}});
    // Same SPW.
    writer.add_spectral_window({.num_chan = 64, .name = "L-band", .ref_frequency = 1.4e9,
                                 .chan_freq = {}, .chan_width = {}, .effective_bw = {},
                                 .resolution = {}, .meas_freq_ref = 0,
                                 .total_bandwidth = 0.0, .net_sideband = 0,
                                 .if_conv_chain = 0, .freq_group = 0,
                                 .freq_group_name = {}, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0,
                                  .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA", .observer = "test2", .project = {},
                             .release_date = 0.0, .flag_row = false});
    writer.add_state({.obs_mode = "CALIBRATE"});

    writer.add_row({.antenna1 = 0, .antenna2 = 1, .array_id = 0,
                     .data_desc_id = 0, .exposure = 0.0, .feed1 = 0, .feed2 = 0,
                     .field_id = 0, .flag_row = false, .interval = 0.0,
                     .observation_id = 0, .processor_id = 0,
                     .scan_number = 2, .state_id = 0, .time = 4.8e9 + 100,
                     .time_centroid = 4.8e9 + 100,
                     .uvw = {300.0, 400.0, 100.0},
                     .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                     .data = {}, .flag = {}});
    writer.add_row({.antenna1 = 0, .antenna2 = 1, .array_id = 0,
                     .data_desc_id = 0, .exposure = 0.0, .feed1 = 0, .feed2 = 0,
                     .field_id = 1, .flag_row = false, .interval = 0.0,
                     .observation_id = 0, .processor_id = 0,
                     .scan_number = 2, .state_id = 0, .time = 4.8e9 + 110,
                     .time_centroid = 4.8e9 + 110,
                     .uvw = {350.0, 450.0, 110.0},
                     .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                     .data = {}, .flag = {}});
    writer.add_row({.antenna1 = 1, .antenna2 = 0, .array_id = 0,
                     .data_desc_id = 0, .exposure = 0.0, .feed1 = 0, .feed2 = 0,
                     .field_id = 1, .flag_row = false, .interval = 0.0,
                     .observation_id = 0, .processor_id = 0,
                     .scan_number = 2, .state_id = 0, .time = 4.8e9 + 120,
                     .time_centroid = 4.8e9 + 120,
                     .uvw = {-350.0, -450.0, -110.0},
                     .sigma = {1.0F, 1.0F}, .weight = {1.0F, 1.0F},
                     .data = {}, .flag = {}});
    writer.flush();
}

static void test_concat_row_count() {
    std::cout << "  concat row count... ";

    auto p1 = make_temp_dir("cat1a");
    auto p2 = make_temp_dir("cat1b");
    auto pout = make_temp_dir("cat1out");
    cleanup(p1); cleanup(p2); cleanup(pout);

    populate_ms1(p1);
    populate_ms2(p2);

    auto ms1 = MeasurementSet::open(p1);
    auto ms2 = MeasurementSet::open(p2);
    auto result = ms_concat(ms1, ms2, pout);

    assert(result.row_count == 5);

    auto out = MeasurementSet::open(pout);
    assert(out.row_count() == 5);

    cleanup(p1); cleanup(p2); cleanup(pout);
    std::cout << "PASS\n";
}

static void test_concat_antenna_dedup() {
    std::cout << "  concat antenna deduplication... ";

    auto p1 = make_temp_dir("cat2a");
    auto p2 = make_temp_dir("cat2b");
    auto pout = make_temp_dir("cat2out");
    cleanup(p1); cleanup(p2); cleanup(pout);

    populate_ms1(p1);
    populate_ms2(p2);

    auto ms1 = MeasurementSet::open(p1);
    auto ms2 = MeasurementSet::open(p2);
    auto result = ms_concat(ms1, ms2, pout);

    auto out = MeasurementSet::open(pout);
    MsMetaData md(out);

    // ms1 has ANT0, ANT1; ms2 has ANT0 (dup), ANT2 (new) → 3 total.
    assert(md.n_antennas() == 3);
    assert(md.antenna_names()[0] == "ANT0");
    assert(md.antenna_names()[1] == "ANT1");
    assert(md.antenna_names()[2] == "ANT2");

    // Check ant remap: ms2 ant0 → 0 (shared), ms2 ant1 → 2 (new).
    assert(result.id_remaps.at("ANTENNA").at(0) == 0);
    assert(result.id_remaps.at("ANTENNA").at(1) == 2);

    cleanup(p1); cleanup(p2); cleanup(pout);
    std::cout << "PASS\n";
}

static void test_concat_field_dedup() {
    std::cout << "  concat field deduplication... ";

    auto p1 = make_temp_dir("cat3a");
    auto p2 = make_temp_dir("cat3b");
    auto pout = make_temp_dir("cat3out");
    cleanup(p1); cleanup(p2); cleanup(pout);

    populate_ms1(p1);
    populate_ms2(p2);

    auto ms1 = MeasurementSet::open(p1);
    auto ms2 = MeasurementSet::open(p2);
    (void)ms_concat(ms1, ms2, pout);

    auto out = MeasurementSet::open(pout);
    MsMetaData md(out);

    // ms1: 3C273; ms2: 3C273 (dup), M31 (new) → 2 total.
    assert(md.n_fields() == 2);
    assert(md.field_names()[0] == "3C273");
    assert(md.field_names()[1] == "M31");

    cleanup(p1); cleanup(p2); cleanup(pout);
    std::cout << "PASS\n";
}

static void test_concat_id_remapping() {
    std::cout << "  concat ID remapping in rows... ";

    auto p1 = make_temp_dir("cat4a");
    auto p2 = make_temp_dir("cat4b");
    auto pout = make_temp_dir("cat4out");
    cleanup(p1); cleanup(p2); cleanup(pout);

    populate_ms1(p1);
    populate_ms2(p2);

    auto ms1 = MeasurementSet::open(p1);
    auto ms2 = MeasurementSet::open(p2);
    (void)ms_concat(ms1, ms2, pout);

    auto out = MeasurementSet::open(pout);
    MsMainColumns cols(out);

    // Row 2 (first ms2 row): ms2 had ant1=0,ant2=1.
    // ms2 ant0 → out ant0 (dedup), ms2 ant1 → out ant2 (new).
    assert(cols.antenna1(2) == 0);
    assert(cols.antenna2(2) == 2);

    // Row 3: ms2 field_id=1 (M31) → out field_id=1 (M31).
    assert(cols.field_id(3) == 1);

    // Row 4: ms2 field_id=1 (M31) → out field_id=1 (M31).
    assert(cols.field_id(4) == 1);

    cleanup(p1); cleanup(p2); cleanup(pout);
    std::cout << "PASS\n";
}

int main() {
    try {
    std::cout << "ms_concat_test\n";

    test_concat_row_count();
    test_concat_antenna_dedup();
    test_concat_field_dedup();
    test_concat_id_remapping();

    std::cout << "all ms_concat tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
