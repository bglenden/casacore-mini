#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/ms_writer.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace casacore_mini;

static fs::path make_temp_dir(const std::string& suffix) {
    auto tmp = fs::temp_directory_path() /
               ("ms_col_test_" + suffix + "_" +
                std::to_string(std::hash<std::string>{}(std::to_string(rand()))));
    return tmp;
}

static void cleanup(const fs::path& p) {
    if (fs::exists(p)) {
        fs::remove_all(p);
    }
}

/// Create an MS with 2 populated rows using MsWriter.
/// Returns the reopened MS for reading.
static MeasurementSet create_populated_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, true);
    MsWriter writer(ms);

    writer.add_antenna({.name = "ANT0",
                        .station = "STN0",
                        .position = {0, 0, 0},
                        .offset = {0, 0, 0},
                        .dish_diameter = 25.0});
    writer.add_antenna({.name = "ANT1",
                        .station = "STN1",
                        .position = {1, 0, 0},
                        .offset = {0, 0, 0},
                        .dish_diameter = 26.0});
    writer.add_spectral_window({.num_chan = 1,
                                .ref_frequency = 1e9,
                                .chan_freq = {1e9},
                                .chan_width = {1e6},
                                .effective_bw = {1e6},
                                .resolution = {1e6},
                                .total_bandwidth = 1e6});
    writer.add_polarization({.num_corr = 1, .corr_type = {5}});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0});
    writer.add_observation({.telescope_name = "TEST"});
    writer.add_state({.obs_mode = "OBSERVE"});
    writer.add_field({.name = "SRC"});

    // Row 0: ant1=0, ant2=1, time=4.8e9, exposure=1.0, flag_row=true.
    writer.add_row({.antenna1 = 0,
                    .antenna2 = 1,
                    .array_id = 0,
                    .data_desc_id = 0,
                    .exposure = 1.0,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = 0,
                    .flag_row = true,
                    .interval = 1.0,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = 1,
                    .state_id = 0,
                    .time = 4.8e9,
                    .time_centroid = 4.8e9,
                    .uvw = {100.0, 101.0, 102.0},
                    .sigma = {0.5F},
                    .weight = {0.5F},
                    .data = {{1.0F, 0.0F}},
                    .flag = {false}});

    // Row 1: ant1=1, ant2=0, time=4.8e9+10, exposure=1.5, flag_row=false.
    writer.add_row({.antenna1 = 1,
                    .antenna2 = 0,
                    .array_id = 0,
                    .data_desc_id = 0,
                    .exposure = 1.5,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = 0,
                    .flag_row = false,
                    .interval = 1.5,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = 2,
                    .state_id = 0,
                    .time = 4.8e9 + 10.0,
                    .time_centroid = 4.8e9 + 10.0,
                    .uvw = {200.0, 201.0, 202.0},
                    .sigma = {1.0F},
                    .weight = {1.0F},
                    .data = {{2.0F, 0.0F}},
                    .flag = {true}});

    writer.flush();
    return MeasurementSet::open(path);
}

static void test_main_scalar_columns() {
    std::cout << "  main table scalar columns... ";

    auto ms_path = make_temp_dir("scalars");
    cleanup(ms_path);

    auto ms = create_populated_ms(ms_path);
    MsMainColumns cols(ms);

    assert(cols.row_count() == 2);

    // Row 0: ant1=0, ant2=1; Row 1: ant1=1, ant2=0.
    assert(cols.antenna1(0) == 0);
    assert(cols.antenna1(1) == 1);
    assert(cols.antenna2(0) == 1);
    assert(cols.antenna2(1) == 0);
    assert(cols.array_id(0) == 0);
    assert(cols.data_desc_id(0) == 0);
    assert(cols.field_id(0) == 0);
    assert(cols.feed1(0) == 0);
    assert(cols.feed2(0) == 0);
    assert(cols.observation_id(0) == 0);
    assert(cols.processor_id(0) == 0);
    assert(cols.scan_number(0) == 1);
    assert(cols.scan_number(1) == 2);
    assert(cols.state_id(0) == 0);

    // TIME/TIME_CENTROID: 4.8e9, 4.8e9+10.
    assert(std::abs(cols.time(0) - 4.8e9) < 1.0);
    assert(std::abs(cols.time(1) - (4.8e9 + 10.0)) < 1.0);
    assert(std::abs(cols.time_centroid(0) - 4.8e9) < 1.0);

    // EXPOSURE/INTERVAL: 1.0, 1.5.
    assert(std::abs(cols.exposure(0) - 1.0) < 1e-10);
    assert(std::abs(cols.exposure(1) - 1.5) < 1e-10);
    assert(std::abs(cols.interval(0) - 1.0) < 1e-10);

    // FLAG_ROW: true for row 0, false for row 1.
    assert(cols.flag_row(0) == true);
    assert(cols.flag_row(1) == false);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_main_array_columns() {
    std::cout << "  main table array columns... ";

    auto ms_path = make_temp_dir("arrays");
    cleanup(ms_path);

    auto ms = create_populated_ms(ms_path);
    MsMainColumns cols(ms);

    // UVW: row 0 = [100, 101, 102], row 1 = [200, 201, 202].
    auto uvw0 = cols.uvw(0);
    assert(uvw0.size() == 3);
    assert(std::abs(uvw0[0] - 100.0) < 1e-10);
    assert(std::abs(uvw0[1] - 101.0) < 1e-10);
    assert(std::abs(uvw0[2] - 102.0) < 1e-10);

    auto uvw1 = cols.uvw(1);
    assert(uvw1.size() == 3);
    assert(std::abs(uvw1[0] - 200.0) < 1e-10);
    assert(std::abs(uvw1[1] - 201.0) < 1e-10);

    // SIGMA: row 0 = [0.5], row 1 = [1.0] (1 correlation).
    auto sig0 = cols.sigma(0);
    assert(sig0.size() == 1);
    assert(std::abs(sig0[0] - 0.5F) < 1e-5F);

    auto sig1 = cols.sigma(1);
    assert(std::abs(sig1[0] - 1.0F) < 1e-5F);

    // WEIGHT: row 0 = [0.5], row 1 = [1.0].
    auto wt0 = cols.weight(0);
    assert(wt0.size() == 1);
    assert(std::abs(wt0[0] - 0.5F) < 1e-5F);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_main_measure_columns() {
    std::cout << "  main table measure columns... ";

    auto ms_path = make_temp_dir("measures");
    cleanup(ms_path);

    auto ms = create_populated_ms(ms_path);
    MsMainColumns cols(ms);

    // TIME measure: MEpoch UTC.
    auto tm = cols.time_measure(0);
    assert(tm.type == MeasureType::epoch);
    auto epoch_ref = std::get<EpochRef>(tm.ref.ref_type);
    assert(epoch_ref == EpochRef::utc);
    auto epoch_val = std::get<EpochValue>(tm.value);
    double total_days = epoch_val.day + epoch_val.fraction;
    assert(total_days > 50000.0);

    // UVW measure: Muvw J2000.
    auto um = cols.uvw_measure(0);
    assert(um.type == MeasureType::uvw);
    auto uvw_ref = std::get<UvwRef>(um.ref.ref_type);
    assert(uvw_ref == UvwRef::j2000);
    auto uvw_val = std::get<UvwValue>(um.value);
    assert(std::abs(uvw_val.u_m - 100.0) < 1e-10);
    assert(std::abs(uvw_val.v_m - 101.0) < 1e-10);
    assert(std::abs(uvw_val.w_m - 102.0) < 1e-10);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_antenna_columns() {
    std::cout << "  ANTENNA subtable columns... ";

    auto ms_path = make_temp_dir("antcols");
    cleanup(ms_path);

    // create_populated_ms already writes 2 antenna rows via MsWriter.
    auto ms = create_populated_ms(ms_path);
    MsAntennaColumns ant_cols(ms);
    assert(ant_cols.row_count() == 2);

    assert(ant_cols.name(0) == "ANT0");
    assert(ant_cols.name(1) == "ANT1");
    assert(ant_cols.station(0) == "STN0");
    assert(ant_cols.station(1) == "STN1");
    assert(ant_cols.type(0) == "GROUND-BASED");
    assert(ant_cols.mount(0) == "ALT-AZ");
    assert(std::abs(ant_cols.dish_diameter(0) - 25.0) < 1e-10);
    assert(std::abs(ant_cols.dish_diameter(1) - 26.0) < 1e-10);
    assert(ant_cols.flag_row(0) == false);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_lazy_open() {
    std::cout << "  lazy SM open on first access... ";

    auto ms_path = make_temp_dir("lazy");
    cleanup(ms_path);

    auto ms = create_populated_ms(ms_path);

    // Construction doesn't open SM.
    MsMainColumns cols(ms);

    // First read triggers SM open.
    assert(cols.antenna1(0) == 0);
    // Subsequent reads reuse it.
    assert(cols.antenna2(0) == 1);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

static void test_empty_ms_columns() {
    std::cout << "  column wrappers on empty MS... ";

    auto ms_path = make_temp_dir("empty");
    cleanup(ms_path);

    auto ms = MeasurementSet::create(ms_path, true);
    MsMainColumns cols(ms);
    assert(cols.row_count() == 0);

    cleanup(ms_path);
    std::cout << "PASS\n";
}

int main() {
    try {
        std::cout << "ms_columns_test\n";

        test_main_scalar_columns();
        test_main_array_columns();
        test_main_measure_columns();
        test_antenna_columns();
        test_lazy_open();
        test_empty_ms_columns();

        std::cout << "all ms_columns tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
