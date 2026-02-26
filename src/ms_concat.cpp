#include "casacore_mini/ms_concat.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/ms_writer.hpp"

namespace casacore_mini {

namespace {

/// Build a name→id map from an MsAntennaColumns.
std::map<std::string, std::int32_t> antenna_name_map(MeasurementSet& ms) {
    MsAntennaColumns cols(ms);
    std::map<std::string, std::int32_t> result;
    for (std::uint64_t r = 0; r < cols.row_count(); ++r) {
        result[cols.name(r)] = static_cast<std::int32_t>(r);
    }
    return result;
}

std::map<std::string, std::int32_t> field_name_map(MeasurementSet& ms) {
    MsFieldColumns cols(ms);
    std::map<std::string, std::int32_t> result;
    for (std::uint64_t r = 0; r < cols.row_count(); ++r) {
        result[cols.name(r)] = static_cast<std::int32_t>(r);
    }
    return result;
}

std::map<std::string, std::int32_t> spw_name_map(MeasurementSet& ms) {
    MsSpWindowColumns cols(ms);
    std::map<std::string, std::int32_t> result;
    for (std::uint64_t r = 0; r < cols.row_count(); ++r) {
        result[cols.name(r)] = static_cast<std::int32_t>(r);
    }
    return result;
}

} // anonymous namespace

MsConcatResult ms_concat(MeasurementSet& ms1, MeasurementSet& ms2,
                         const std::filesystem::path& output_path) {
    MsConcatResult result;

    // Create the output MS.
    auto out = MeasurementSet::create(output_path, false);
    MsWriter writer(out);

    // --- Merge ANTENNA subtables ---
    // Copy all antennas from ms1, then add ms2 antennas (dedup by name).
    auto ant1_names = antenna_name_map(ms1);
    MsAntennaColumns ant1(ms1);
    MsAntennaColumns ant2(ms2);

    std::map<std::int32_t, std::int32_t> ant_remap;
    // Copy ms1 antennas.
    for (std::uint64_t r = 0; r < ant1.row_count(); ++r) {
        writer.add_antenna({.name = ant1.name(r),
                            .station = ant1.station(r),
                            .type = ant1.type(r),
                            .mount = ant1.mount(r),
                            .position = {},
                            .offset = {},
                            .dish_diameter = ant1.dish_diameter(r)});
    }
    // Add ms2 antennas, remapping IDs.
    for (std::uint64_t r = 0; r < ant2.row_count(); ++r) {
        auto name = ant2.name(r);
        auto it = ant1_names.find(name);
        if (it != ant1_names.end()) {
            // Duplicate → remap to existing ID.
            ant_remap[static_cast<std::int32_t>(r)] = it->second;
        } else {
            auto new_id = writer.add_antenna({.name = name,
                                              .station = ant2.station(r),
                                              .type = ant2.type(r),
                                              .mount = ant2.mount(r),
                                              .position = {},
                                              .offset = {},
                                              .dish_diameter = ant2.dish_diameter(r)});
            ant_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(new_id);
        }
    }

    // --- Merge FIELD subtables ---
    auto fld1_names = field_name_map(ms1);
    MsFieldColumns fld1(ms1);
    MsFieldColumns fld2(ms2);

    std::map<std::int32_t, std::int32_t> field_remap;
    for (std::uint64_t r = 0; r < fld1.row_count(); ++r) {
        writer.add_field({.name = fld1.name(r),
                          .code = fld1.code(r),
                          .time = fld1.time(r),
                          .source_id = fld1.source_id(r)});
    }
    for (std::uint64_t r = 0; r < fld2.row_count(); ++r) {
        auto name = fld2.name(r);
        auto it = fld1_names.find(name);
        if (it != fld1_names.end()) {
            field_remap[static_cast<std::int32_t>(r)] = it->second;
        } else {
            auto new_id = writer.add_field({.name = name,
                                            .code = fld2.code(r),
                                            .time = fld2.time(r),
                                            .source_id = fld2.source_id(r)});
            field_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(new_id);
        }
    }

    // --- Merge SPW subtables ---
    auto spw1_names = spw_name_map(ms1);
    MsSpWindowColumns spw1(ms1);
    MsSpWindowColumns spw2(ms2);

    std::map<std::int32_t, std::int32_t> spw_remap;
    for (std::uint64_t r = 0; r < spw1.row_count(); ++r) {
        writer.add_spectral_window({.num_chan = spw1.num_chan(r),
                                    .name = spw1.name(r),
                                    .ref_frequency = spw1.ref_frequency(r),
                                    .chan_freq = {},
                                    .chan_width = {},
                                    .effective_bw = {},
                                    .resolution = {},
                                    .meas_freq_ref = 0,
                                    .total_bandwidth = 0.0,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = {}});
    }
    for (std::uint64_t r = 0; r < spw2.row_count(); ++r) {
        auto name = spw2.name(r);
        auto it = spw1_names.find(name);
        if (it != spw1_names.end()) {
            spw_remap[static_cast<std::int32_t>(r)] = it->second;
        } else {
            auto new_id = writer.add_spectral_window({.num_chan = spw2.num_chan(r),
                                                      .name = name,
                                                      .ref_frequency = spw2.ref_frequency(r),
                                                      .chan_freq = {},
                                                      .chan_width = {},
                                                      .effective_bw = {},
                                                      .resolution = {},
                                                      .meas_freq_ref = 0,
                                                      .total_bandwidth = 0.0,
                                                      .net_sideband = 0,
                                                      .if_conv_chain = 0,
                                                      .freq_group = 0,
                                                      .freq_group_name = {}});
            spw_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(new_id);
        }
    }

    // --- Build DD remap via SPW remap ---
    // Copy DD rows from ms1, then remap+add from ms2.
    MsDataDescColumns dd1(ms1);
    MsDataDescColumns dd2(ms2);

    std::map<std::int32_t, std::int32_t> dd_remap;
    for (std::uint64_t r = 0; r < dd1.row_count(); ++r) {
        writer.add_data_description({.spectral_window_id = dd1.spectral_window_id(r),
                                     .polarization_id = dd1.polarization_id(r)});
    }
    for (std::uint64_t r = 0; r < dd2.row_count(); ++r) {
        auto old_spw = dd2.spectral_window_id(r);
        auto new_spw = spw_remap.count(old_spw) > 0 ? spw_remap.at(old_spw) : old_spw;
        // Check if this DD already exists in ms1.
        bool found = false;
        for (std::uint64_t d1 = 0; d1 < dd1.row_count(); ++d1) {
            if (dd1.spectral_window_id(d1) == new_spw &&
                dd1.polarization_id(d1) == dd2.polarization_id(r)) {
                dd_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(d1);
                found = true;
                break;
            }
        }
        if (!found) {
            auto new_id = writer.add_data_description(
                {.spectral_window_id = new_spw, .polarization_id = dd2.polarization_id(r)});
            dd_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(new_id);
        }
    }

    // --- Copy POLARIZATION from ms1 (assume compatible) ---
    MsPolarizationColumns pol1(ms1);
    for (std::uint64_t r = 0; r < pol1.row_count(); ++r) {
        writer.add_polarization({.num_corr = pol1.num_corr(r), .corr_type = {}});
    }

    // --- Copy OBSERVATION from ms1, add unique from ms2 ---
    MsObservationColumns obs1(ms1);
    MsObservationColumns obs2(ms2);
    for (std::uint64_t r = 0; r < obs1.row_count(); ++r) {
        writer.add_observation({.telescope_name = obs1.telescope_name(r),
                                .observer = obs1.observer(r),
                                .project = {}});
    }
    for (std::uint64_t r = 0; r < obs2.row_count(); ++r) {
        writer.add_observation({.telescope_name = obs2.telescope_name(r),
                                .observer = obs2.observer(r),
                                .project = {}});
    }

    // --- Copy STATE from ms1, add from ms2 ---
    {
        auto& state1 = ms1.subtable("STATE");
        for (std::uint64_t r = 0; r < state1.nrow(); ++r) {
            auto obs_mode = std::get<std::string>(state1.read_scalar_cell("OBS_MODE", r));
            auto sig_val = std::get<bool>(state1.read_scalar_cell("SIG", r));
            auto ref_val = std::get<bool>(state1.read_scalar_cell("REF", r));
            writer.add_state({.sig = sig_val,
                              .ref = ref_val,
                              .cal = 0.0,
                              .load = 0.0,
                              .sub_scan = 0,
                              .obs_mode = obs_mode});
        }
    }

    std::map<std::int32_t, std::int32_t> state_remap;
    {
        auto& state2 = ms2.subtable("STATE");
        for (std::uint64_t r = 0; r < state2.nrow(); ++r) {
            auto obs_mode = std::get<std::string>(state2.read_scalar_cell("OBS_MODE", r));
            auto sig_val = std::get<bool>(state2.read_scalar_cell("SIG", r));
            auto ref_val = std::get<bool>(state2.read_scalar_cell("REF", r));
            auto new_id = writer.add_state({.sig = sig_val,
                                            .ref = ref_val,
                                            .cal = 0.0,
                                            .load = 0.0,
                                            .sub_scan = 0,
                                            .obs_mode = obs_mode});
            state_remap[static_cast<std::int32_t>(r)] = static_cast<std::int32_t>(new_id);
        }
    }

    // --- Copy main-table rows ---
    MsMainColumns cols1(ms1);
    for (std::uint64_t r = 0; r < ms1.row_count(); ++r) {
        writer.add_row({.antenna1 = cols1.antenna1(r),
                        .antenna2 = cols1.antenna2(r),
                        .array_id = cols1.array_id(r),
                        .data_desc_id = cols1.data_desc_id(r),
                        .exposure = cols1.exposure(r),
                        .feed1 = cols1.feed1(r),
                        .feed2 = cols1.feed2(r),
                        .field_id = cols1.field_id(r),
                        .flag_row = cols1.flag_row(r),
                        .interval = cols1.interval(r),
                        .observation_id = cols1.observation_id(r),
                        .processor_id = cols1.processor_id(r),
                        .scan_number = cols1.scan_number(r),
                        .state_id = cols1.state_id(r),
                        .time = cols1.time(r),
                        .time_centroid = cols1.time_centroid(r),
                        .uvw = cols1.uvw(r),
                        .sigma = cols1.sigma(r),
                        .weight = cols1.weight(r),
                        .data = {},
                        .flag = {}});
    }

    // ms2 rows with remapped IDs.
    MsMainColumns cols2(ms2);
    for (std::uint64_t r = 0; r < ms2.row_count(); ++r) {
        auto old_ant1 = cols2.antenna1(r);
        auto old_ant2 = cols2.antenna2(r);
        auto old_fld = cols2.field_id(r);
        auto old_dd = cols2.data_desc_id(r);
        auto old_state = cols2.state_id(r);

        writer.add_row(
            {.antenna1 = ant_remap.count(old_ant1) > 0 ? ant_remap.at(old_ant1) : old_ant1,
             .antenna2 = ant_remap.count(old_ant2) > 0 ? ant_remap.at(old_ant2) : old_ant2,
             .array_id = cols2.array_id(r),
             .data_desc_id = dd_remap.count(old_dd) > 0 ? dd_remap.at(old_dd) : old_dd,
             .exposure = cols2.exposure(r),
             .feed1 = cols2.feed1(r),
             .feed2 = cols2.feed2(r),
             .field_id = field_remap.count(old_fld) > 0 ? field_remap.at(old_fld) : old_fld,
             .flag_row = cols2.flag_row(r),
             .interval = cols2.interval(r),
             .observation_id = cols2.observation_id(r),
             .processor_id = cols2.processor_id(r),
             .scan_number = cols2.scan_number(r),
             .state_id = state_remap.count(old_state) > 0 ? state_remap.at(old_state) : old_state,
             .time = cols2.time(r),
             .time_centroid = cols2.time_centroid(r),
             .uvw = cols2.uvw(r),
             .sigma = cols2.sigma(r),
             .weight = cols2.weight(r),
             .data = {},
             .flag = {}});
    }

    writer.flush();

    result.row_count = ms1.row_count() + ms2.row_count();
    result.id_remaps["ANTENNA"] = ant_remap;
    result.id_remaps["FIELD"] = field_remap;
    result.id_remaps["SPECTRAL_WINDOW"] = spw_remap;
    result.id_remaps["DATA_DESCRIPTION"] = dd_remap;
    result.id_remaps["STATE"] = state_remap;

    return result;
}

} // namespace casacore_mini
