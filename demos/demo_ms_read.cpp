// demo_ms_read.cpp -- Phase 9: Read MS, iterate, select
//
// casacore-original equivalent:
//   MeasurementSet ms("test.ms");
//   MSMainColumns mcols(ms);
//   cout << mcols.antenna1()(0) << endl;
//
//   MSIter iter(ms, {"ARRAY_ID", "FIELD_ID", "DATA_DESC_ID"});
//   for (iter.origin(); iter.more(); iter++) { ... }
//
//   MSSelection sel;
//   sel.setAntennaExpr("0,1");
//   sel.toTableExprNode(&ms);
//   // NOTE: casacore returns a filtered Table view; casacore-mini returns
//   // row indices. This is a documented API gap.

#include <casacore_mini/measurement_set.hpp>
#include <casacore_mini/ms_columns.hpp>
#include <casacore_mini/ms_iter.hpp>
#include <casacore_mini/ms_selection.hpp>
#include <casacore_mini/ms_writer.hpp>

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace casacore_mini;
namespace fs = std::filesystem;

static fs::path make_temp_path() {
    return fs::temp_directory_path() / "casacore_mini_demo_ms_read.ms";
}

/// Create a small test MS (reuses the write pattern from demo_ms_write).
static void create_test_ms(const fs::path& path) {
    auto ms = MeasurementSet::create(path, true);
    MsWriter writer(ms);

    writer.add_antenna({.name = "ANT0",
                        .station = "W01",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {-1601185.0, -5041978.0, 3554876.0},
                        .offset = {0.0, 0.0, 0.0},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT1",
                        .station = "W02",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {-1601192.0, -5041981.0, 3554871.0},
                        .offset = {0.0, 0.0, 0.0},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_antenna({.name = "ANT2",
                        .station = "W03",
                        .type = "GROUND-BASED",
                        .mount = "ALT-AZ",
                        .position = {-1601200.0, -5041985.0, 3554866.0},
                        .offset = {0.0, 0.0, 0.0},
                        .dish_diameter = 25.0,
                        .flag_row = false});

    writer.add_spectral_window({.num_chan = 64,
                                .name = "L-band",
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
    writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});
    writer.add_field({.name = "3C286",
                      .code = "C",
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA",
                            .observer = "demo",
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.obs_mode = "OBSERVE_TARGET.ON_SOURCE"});

    double t0 = 4.8e9;
    double dt = 10.0;
    int baselines[][2] = {{0, 1}, {0, 2}, {1, 2}};

    for (int ti = 0; ti < 2; ++ti) {
        for (int bi = 0; bi < 3; ++bi) {
            writer.add_row({.antenna1 = baselines[bi][0],
                            .antenna2 = baselines[bi][1],
                            .array_id = 0,
                            .data_desc_id = 0,
                            .exposure = dt,
                            .feed1 = 0,
                            .feed2 = 0,
                            .field_id = 0,
                            .flag_row = false,
                            .interval = dt,
                            .observation_id = 0,
                            .processor_id = 0,
                            .scan_number = ti + 1,
                            .state_id = 0,
                            .time = t0 + static_cast<double>(ti) * dt,
                            .time_centroid = t0 + static_cast<double>(ti) * dt,
                            .uvw = {100.0, 200.0, 50.0},
                            .sigma = {1.0F, 1.0F},
                            .weight = {1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }
    }
    writer.flush();
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: MS Read (Phase 9) ===\n";

        auto ms_path = make_temp_path();
        if (fs::exists(ms_path)) {
            fs::remove_all(ms_path);
        }

        // 1. Create test MS.
        std::cout << "\n--- Creating test MS ---\n";
        create_test_ms(ms_path);
        auto ms = MeasurementSet::open(ms_path);
        std::cout << "  Opened MS: " << ms.path() << "\n";
        std::cout << "  Main rows: " << ms.row_count() << "\n";
        assert(ms.row_count() == 6);

        // 2. Typed column access.
        std::cout << "\n--- Typed Column Access (MsMainColumns) ---\n";
        MsMainColumns main_cols(ms);
        for (std::uint64_t r = 0; r < ms.row_count(); ++r) {
            std::cout << "  row " << r << ": ANT1=" << main_cols.antenna1(r)
                      << " ANT2=" << main_cols.antenna2(r) << " SCAN=" << main_cols.scan_number(r)
                      << " TIME=" << main_cols.time(r) << "\n";
        }

        // 3. Subtable access.
        std::cout << "\n--- Subtable Access (MsAntennaColumns) ---\n";
        MsAntennaColumns ant_cols(ms);
        for (std::uint64_t r = 0; r < ant_cols.row_count(); ++r) {
            std::cout << "  ANT " << r << ": name=" << ant_cols.name(r)
                      << " station=" << ant_cols.station(r) << " dish=" << ant_cols.dish_diameter(r)
                      << " m\n";
        }

        // 4. Iteration via MsIter.
        std::cout << "\n--- Iteration (ms_iter_chunks) ---\n";
        auto chunks = ms_iter_chunks(ms);
        std::cout << "  " << chunks.size() << " iteration chunks\n";
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            std::cout << "  chunk " << i << ": array=" << chunk.array_id
                      << " field=" << chunk.field_id << " ddid=" << chunk.data_desc_id
                      << " rows=" << chunk.rows.size() << "\n";
        }
        // All 6 rows should be in one chunk (same array/field/ddid).
        assert(chunks.size() == 1);
        assert(chunks[0].rows.size() == 6);

        // Time-based iteration.
        std::cout << "\n--- Time-Based Iteration (ms_time_chunks, 10s bins) ---\n";
        auto time_chunks = ms_time_chunks(ms, 10.0);
        std::cout << "  " << time_chunks.size() << " time chunks\n";
        for (std::size_t i = 0; i < time_chunks.size(); ++i) {
            std::cout << "  chunk " << i << ": time_center=" << time_chunks[i].time_center
                      << " rows=" << time_chunks[i].rows.size() << "\n";
        }
        // 2 timestamps -> 2 time chunks.
        assert(time_chunks.size() == 2);
        assert(time_chunks[0].rows.size() == 3);
        assert(time_chunks[1].rows.size() == 3);

        // 5. Selection: antenna expression.
        std::cout << "\n--- Selection (MsSelection) ---\n";
        {
            MsSelection sel;
            sel.set_antenna_expr("0");
            auto result = sel.evaluate(ms);
            std::cout << "  antenna='0': " << result.rows.size() << " rows selected\n";
            // Antenna 0 appears in baselines 0-1 and 0-2, so 4 rows (2 timestamps).
            assert(result.rows.size() == 4);
        }

        // Selection: scan expression.
        {
            MsSelection sel;
            sel.set_scan_expr("1");
            auto result = sel.evaluate(ms);
            std::cout << "  scan='1':    " << result.rows.size() << " rows selected\n";
            assert(result.rows.size() == 3);
        }

        // Combined selection.
        {
            MsSelection sel;
            sel.set_antenna_expr("0&1"); // baseline 0-1 only
            sel.set_scan_expr("1");
            auto result = sel.evaluate(ms);
            std::cout << "  antenna='0&1' AND scan='1': " << result.rows.size() << " rows\n";
            assert(result.rows.size() == 1);
        }

        std::cout << "\n  NOTE: casacore-original MsSelection returns a filtered Table\n"
                  << "        view with direct iteration. casacore-mini returns row\n"
                  << "        indices, requiring the caller to index into the original\n"
                  << "        table. This is a documented API gap.\n";

        // Clean up.
        fs::remove_all(ms_path);
        std::cout << "\n=== MS Read demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
