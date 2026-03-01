#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_iter.hpp"
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
               ("ms_iter_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Create a test MS with rows spanning multiple fields and data descriptions.
static MeasurementSet create_multi_group_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, false);
    MsWriter writer(ms);

    writer.add_antenna({.name = "A0",
                        .station = {},
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 0.0,
                        .flag_row = false});
    writer.add_antenna({.name = "A1",
                        .station = {},
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {},
                        .offset = {},
                        .dish_diameter = 0.0,
                        .flag_row = false});
    writer.add_field(
        {.name = "F0", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_field(
        {.name = "F1", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_data_description({});
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
    writer.add_state(
        {.sig = true, .ref = false, .cal = 0.0, .load = 0.0, .sub_scan = 0, .obs_mode = "OBSERVE"});

    // Rows: field 0 dd 0, field 0 dd 1, field 1 dd 0.
    MsMainRow base{};
    base.antenna1 = 0;
    base.antenna2 = 1;
    base.scan_number = 1;
    base.uvw = {0.0, 0.0, 0.0};
    base.sigma = {1.0F};
    base.weight = {1.0F};

    // 3 rows: field=0 dd=0 at t=100.
    base.field_id = 0;
    base.data_desc_id = 0;
    base.time = 100.0;
    base.time_centroid = 100.0;
    writer.add_row(base);

    // field=0 dd=1 at t=110.
    base.data_desc_id = 1;
    base.time = 110.0;
    base.time_centroid = 110.0;
    writer.add_row(base);

    // field=1 dd=0 at t=200.
    base.field_id = 1;
    base.data_desc_id = 0;
    base.time = 200.0;
    base.time_centroid = 200.0;
    writer.add_row(base);

    // Another field=0 dd=0 at t=120.
    base.field_id = 0;
    base.data_desc_id = 0;
    base.time = 120.0;
    base.time_centroid = 120.0;
    writer.add_row(base);

    writer.flush();
    return MeasurementSet::open(path);
}

static void test_iter_chunks() {
    std::cout << "  ms_iter_chunks grouping... ";

    auto ms_path = make_temp_dir("chunks");
    cleanup(ms_path);

    auto ms = create_multi_group_ms(ms_path);
    auto chunks = ms_iter_chunks(ms);

    // We have 3 unique (array_id, field_id, data_desc_id) groups:
    // (0, 0, 0) -> rows 0, 3
    // (0, 0, 1) -> row 1
    // (0, 1, 0) -> row 2
    assert(chunks.size() == 3);

    // Find the (0,0,0) chunk.
    bool found_00 = false;
    for (const auto& c : chunks) {
        if (c.array_id == 0 && c.field_id == 0 && c.data_desc_id == 0) {
            assert(c.rows.size() == 2);
            assert(c.rows[0] == 0);
            assert(c.rows[1] == 3);
            found_00 = true;
        }
    }
    (void)found_00;
    assert(found_00);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_time_chunks() {
    std::cout << "  ms_time_chunks binning... ";

    auto ms_path = make_temp_dir("time");
    cleanup(ms_path);

    auto ms = create_multi_group_ms(ms_path);

    // Times are: 100, 110, 200, 120.
    // With interval=50: bin 0 covers [100, 150) -> rows 0, 1, 3.
    //                    bin 2 covers [200, 250) -> row 2.
    auto chunks = ms_time_chunks(ms, 50.0);
    assert(chunks.size() == 2);

    // First chunk should have 3 rows.
    assert(chunks[0].rows.size() == 3);
    // Second chunk should have 1 row.
    assert(chunks[1].rows.size() == 1);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_empty_ms_iter() {
    std::cout << "  empty MS iteration... ";

    auto ms_path = make_temp_dir("empty");
    cleanup(ms_path);

    auto ms = MeasurementSet::create(ms_path, false);
    auto chunks = ms_iter_chunks(ms);
    assert(chunks.empty());

    auto time_chunks = ms_time_chunks(ms, 10.0);
    assert(time_chunks.empty());

    cleanup(ms_path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_iter_test\n";

        test_iter_chunks();
        test_time_chunks();
        test_empty_ms_iter();

        std::cout << "all ms_iter tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
