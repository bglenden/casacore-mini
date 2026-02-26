// demo_ms_write.cpp -- Phase 9: Create MeasurementSet from scratch
//
// casacore-original equivalent (writems.cc simplified):
//   MeasurementSet ms(MeasurementSet::requiredTableDesc(), SetupNewTable("test.ms",
//       MeasurementSet::requiredTableDesc(), Table::New));
//   ms.createDefaultSubtables(Table::New);
//   MSAntennaColumns antcols(ms.antenna());
//   antcols.name().put(0, "ANT0");
//   MSMainColumns mcols(ms);
//   mcols.antenna1().put(0, 0);
//   mcols.time().put(0, 4.8e9);
//   ...

#include <casacore_mini/measurement_set.hpp>
#include <casacore_mini/ms_columns.hpp>
#include <casacore_mini/ms_writer.hpp>

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

using namespace casacore_mini;
namespace fs = std::filesystem;

static fs::path make_temp_path() {
    return fs::temp_directory_path() / "casacore_mini_demo_ms_write.ms";
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: MS Write (Phase 9) ===\n";

        auto ms_path = make_temp_path();
        if (fs::exists(ms_path)) {
            fs::remove_all(ms_path);
        }

        // 1. Create MS structure.
        std::cout << "\n--- Creating MeasurementSet ---\n";
        auto ms = MeasurementSet::create(ms_path, true);
        MsWriter writer(ms);

        // 2. Add 3 VLA-like antennas.
        std::cout << "  Adding 3 antennas (VLA-like)...\n";
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

        // 3. Spectral window: 64 channels at 1.4 GHz L-band.
        std::cout << "  Adding spectral window (64 channels, 1.4 GHz)...\n";
        writer.add_spectral_window({.num_chan = 64,
                                    .name = "L-band",
                                    .ref_frequency = 1.4e9,
                                    .chan_freq = {},
                                    .chan_width = {},
                                    .effective_bw = {},
                                    .resolution = {},
                                    .meas_freq_ref = 5, // TOPO
                                    .total_bandwidth = 2.0e6,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = {},
                                    .flag_row = false});

        // 4. RR/LL polarization.
        std::cout << "  Adding polarization (RR/LL)...\n";
        writer.add_polarization({.num_corr = 2, .corr_type = {5, 8}, .flag_row = false});

        // 5. Field: 3C286.
        std::cout << "  Adding field (3C286)...\n";
        writer.add_field({.name = "3C286",
                          .code = "C",
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});

        // 6. Data description.
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});

        // 7. Observation.
        writer.add_observation({.telescope_name = "VLA",
                                .observer = "demo",
                                .project = {},
                                .release_date = 0.0,
                                .flag_row = false});

        // 8. State.
        writer.add_state({.obs_mode = "OBSERVE_TARGET.ON_SOURCE"});

        // 9. Main rows: 3 baselines x 2 timestamps = 6 rows.
        std::cout << "  Adding 6 main-table rows (3 baselines x 2 timestamps)...\n";
        double t0 = 4.8e9; // MJD in seconds
        double dt = 10.0;

        // Baselines: 0-1, 0-2, 1-2
        int baselines[][2] = {{0, 1}, {0, 2}, {1, 2}};
        double uvws[][3] = {{100.0, 200.0, 50.0}, {150.0, -100.0, 75.0}, {-50.0, 300.0, -25.0}};

        for (int ti = 0; ti < 2; ++ti) {
            for (int bi = 0; bi < 3; ++bi) {
                double sign = (ti == 0) ? 1.0 : -1.0;
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
                                .uvw = {sign * uvws[bi][0], sign * uvws[bi][1], sign * uvws[bi][2]},
                                .sigma = {1.0F, 1.0F},
                                .weight = {1.0F, 1.0F},
                                .data = {},
                                .flag = {}});
            }
        }

        // 10. Flush to disk.
        std::cout << "  Flushing to disk...\n";
        writer.flush();
        std::cout << "  MS created at: " << ms_path << "\n";

        // 11. Verify structure.
        std::cout << "\n--- Verifying MS ---\n";
        auto ms2 = MeasurementSet::open(ms_path);
        std::cout << "  Main table rows: " << ms2.row_count() << "\n";
        assert(ms2.row_count() == 6);

        std::cout << "  Subtables: ";
        for (const auto& name : ms2.subtable_names()) {
            std::cout << name << " ";
        }
        std::cout << "\n";

        // Verify antenna subtable.
        MsAntennaColumns ant_cols(ms2);
        std::cout << "  Antennas: " << ant_cols.row_count() << "\n";
        assert(ant_cols.row_count() == 3);
        assert(ant_cols.name(0) == "ANT0");
        assert(ant_cols.name(1) == "ANT1");
        assert(ant_cols.name(2) == "ANT2");
        assert(std::fabs(ant_cols.dish_diameter(0) - 25.0) < 1e-10);

        // Verify main table scalars.
        MsMainColumns main_cols(ms2);
        assert(main_cols.antenna1(0) == 0);
        assert(main_cols.antenna2(0) == 1);
        assert(main_cols.scan_number(0) == 1);
        assert(main_cols.scan_number(3) == 2);
        assert(std::fabs(main_cols.time(0) - t0) < 1e-6);

        // Verify spectral window.
        MsSpWindowColumns spw_cols(ms2);
        assert(spw_cols.row_count() == 1);
        assert(spw_cols.num_chan(0) == 64);
        assert(spw_cols.name(0) == "L-band");
        assert(std::fabs(spw_cols.ref_frequency(0) - 1.4e9) < 1e-3);

        // Verify field.
        MsFieldColumns field_cols(ms2);
        assert(field_cols.row_count() == 1);
        assert(field_cols.name(0) == "3C286");

        std::cout << "  [OK] All MS structure verified.\n";

        // Clean up.
        fs::remove_all(ms_path);
        std::cout << "\n=== MS Write demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
