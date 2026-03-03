// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// demo_ms_select.cpp -- Phase 9: MS selection transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/ms/apps/msselect.cc
//     MeasurementSet ms(msin);
//     MSSelection select;
//     select.setAntennaExpr(baseline);
//     TableExprNode node = select.toTableExprNode(&ms);
//     Table mssel = ms(node);
//     if (deep) {
//       mssel.deepCopy(out, Table::New);
//     } else {
//       mssel.rename(out, Table::New);
//     }
//
// This casacore-mini demo transliterates the selection flow using MsSelection.
// It materializes selected rows into a text report artifact.

#include <casacore_mini/measurement_set.hpp>
#include <casacore_mini/ms_columns.hpp>
#include <casacore_mini/ms_selection.hpp>
#include <casacore_mini/ms_writer.hpp>

#include "demo_check.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace casacore_mini;
namespace fs = std::filesystem;

static fs::path make_temp_path() {
    return fs::temp_directory_path() / "casacore_mini_demo_ms_select.ms";
}

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

    const double t0 = 4.8e9;
    const double dt = 10.0;
    const int baselines[][2] = {{0, 1}, {0, 2}, {1, 2}};

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

static void write_selection_report(const fs::path& report_path, MeasurementSet& ms,
                                   const MsSelectionResult& selection) {
    std::ofstream out(report_path);
    if (!out) {
        throw std::runtime_error("failed to open report path for writing");
    }
    MsMainColumns cols(ms);
    out << "Selected rows: " << selection.rows.size() << "\n";
    for (const auto row : selection.rows) {
        out << "row=" << row << " ant1=" << cols.antenna1(row) << " ant2=" << cols.antenna2(row)
            << " scan=" << cols.scan_number(row) << " field=" << cols.field_id(row) << "\n";
    }
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: MS Select (Phase 9) ===\n";

        auto ms_path = make_temp_path();
        if (fs::exists(ms_path)) {
            fs::remove_all(ms_path);
        }

        create_test_ms(ms_path);
        auto ms = MeasurementSet::open(ms_path);
        DEMO_CHECK(ms.row_count() == 6);

        std::cout << "\n--- Baseline selection: 0&1 ---\n";
        MsSelection baseline_sel;
        baseline_sel.set_antenna_expr("0&1");
        auto baseline_rows = baseline_sel.evaluate(ms);
        std::cout << "  selected rows: " << baseline_rows.rows.size() << "\n";
        DEMO_CHECK(baseline_rows.rows.size() == 2);

        std::cout << "\n--- Scan selection: 1 ---\n";
        MsSelection scan_sel;
        scan_sel.set_scan_expr("1");
        auto scan_rows = scan_sel.evaluate(ms);
        std::cout << "  selected rows: " << scan_rows.rows.size() << "\n";
        DEMO_CHECK(scan_rows.rows.size() == 3);

        std::cout << "\n--- Combined selection: antenna=0&1 AND scan=1 ---\n";
        MsSelection combined_sel;
        combined_sel.set_antenna_expr("0&1");
        combined_sel.set_scan_expr("1");
        auto combined_rows = combined_sel.evaluate(ms);
        std::cout << "  selected rows: " << combined_rows.rows.size() << "\n";
        DEMO_CHECK(combined_rows.rows.size() == 1);

        const auto report = ms_path.parent_path() / "casacore_mini_demo_ms_select_rows.txt";
        write_selection_report(report, ms, combined_rows);
        DEMO_CHECK(fs::exists(report));
        std::cout << "  wrote selection report: " << report << "\n";

        fs::remove(report);
        fs::remove_all(ms_path);
        std::cout << "\n=== MS Select demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
