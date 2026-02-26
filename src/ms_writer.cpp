#include "casacore_mini/ms_writer.hpp"

#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_desc_writer.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace casacore_mini {

MsWriter::MsWriter(MeasurementSet& ms) : ms_(ms) {}

void MsWriter::add_row(MsMainRow row) { main_rows_.push_back(std::move(row)); }

std::uint64_t MsWriter::add_antenna(MsAntennaRow row) {
    auto idx = antenna_rows_.size();
    antenna_rows_.push_back(std::move(row));
    return idx;
}

std::uint64_t MsWriter::add_spectral_window(MsSpWindowRow row) {
    auto idx = spw_rows_.size();
    spw_rows_.push_back(std::move(row));
    return idx;
}

std::uint64_t MsWriter::add_field(MsFieldRow row) {
    auto idx = field_rows_.size();
    field_rows_.push_back(std::move(row));
    return idx;
}

std::uint64_t MsWriter::add_data_description(MsDataDescRow row) {
    auto idx = dd_rows_.size();
    dd_rows_.push_back(row);
    return idx;
}

std::uint64_t MsWriter::add_polarization(MsPolarizationRow row) {
    auto idx = pol_rows_.size();
    pol_rows_.push_back(std::move(row));
    return idx;
}

std::uint64_t MsWriter::add_observation(MsObservationRow row) {
    auto idx = obs_rows_.size();
    obs_rows_.push_back(std::move(row));
    return idx;
}

std::uint64_t MsWriter::add_state(MsStateRow row) {
    auto idx = state_rows_.size();
    state_rows_.push_back(std::move(row));
    return idx;
}

void MsWriter::validate_foreign_keys() const {
    std::string errors;
    const auto n_ant = antenna_rows_.size();
    const auto n_field = field_rows_.size();
    const auto n_dd = dd_rows_.size();
    const auto n_obs = obs_rows_.size();
    const auto n_state = state_rows_.size();

    for (std::size_t r = 0; r < main_rows_.size(); ++r) {
        const auto& row = main_rows_[r];
        if (row.antenna1 < 0 || static_cast<std::size_t>(row.antenna1) >= n_ant) {
            errors += "row " + std::to_string(r) + ": ANTENNA1=" +
                      std::to_string(row.antenna1) + " out of range [0," +
                      std::to_string(n_ant) + ")\n";
        }
        if (row.antenna2 < 0 || static_cast<std::size_t>(row.antenna2) >= n_ant) {
            errors += "row " + std::to_string(r) + ": ANTENNA2=" +
                      std::to_string(row.antenna2) + " out of range\n";
        }
        if (row.field_id < 0 || static_cast<std::size_t>(row.field_id) >= n_field) {
            errors += "row " + std::to_string(r) + ": FIELD_ID=" +
                      std::to_string(row.field_id) + " out of range\n";
        }
        if (row.data_desc_id < 0 || static_cast<std::size_t>(row.data_desc_id) >= n_dd) {
            errors += "row " + std::to_string(r) + ": DATA_DESC_ID=" +
                      std::to_string(row.data_desc_id) + " out of range\n";
        }
        if (row.observation_id < 0 || static_cast<std::size_t>(row.observation_id) >= n_obs) {
            errors += "row " + std::to_string(r) + ": OBSERVATION_ID=" +
                      std::to_string(row.observation_id) + " out of range\n";
        }
        if (row.state_id < 0 || static_cast<std::size_t>(row.state_id) >= n_state) {
            errors += "row " + std::to_string(r) + ": STATE_ID=" +
                      std::to_string(row.state_id) + " out of range\n";
        }
    }

    if (!errors.empty()) {
        throw std::runtime_error("Foreign key validation failed:\n" + errors);
    }
}

void MsWriter::flush_subtable(
    std::string_view name, const std::vector<ColumnDesc>& columns, std::uint64_t nrows,
    const std::function<CellValue(std::size_t col, std::uint64_t row)>& get_cell) {

    const auto st_path = ms_.path() / std::string(name);

    // Build column specs from descriptors.
    std::vector<TableColumnSpec> specs;
    for (const auto& col : columns) {
        TableColumnSpec spec;
        spec.name = col.name;
        spec.data_type = col.data_type;
        spec.kind = col.kind;
        spec.shape.assign(col.shape.begin(), col.shape.end());
        spec.comment = col.comment;
        specs.push_back(std::move(spec));
    }

    // Remove old subtable and create fresh.
    std::filesystem::remove_all(st_path);
    auto table = Table::create(st_path, specs, nrows);

    for (std::size_t ci = 0; ci < columns.size(); ++ci) {
        if (columns[ci].kind == ColumnKind::array) {
            continue;
        }
        for (std::uint64_t r = 0; r < nrows; ++r) {
            table.write_scalar_cell(columns[ci].name, r, get_cell(ci, r));
        }
    }

    table.flush();

    // Write table.info for subtable.
    write_table_info(st_path, "", "");
}

void MsWriter::flush_main_table() {
    if (main_rows_.empty()) {
        return;
    }

    const auto nrows = static_cast<std::uint64_t>(main_rows_.size());
    auto table_dat = ms_.main_table().table_dat();
    table_dat.row_count = nrows;
    table_dat.post_td_row_count = nrows;

    // Partition columns into SSM vs TSM based on dm_type in table.dat.
    std::vector<ColumnDesc> ssm_cols;
    std::vector<ColumnDesc> tsm_cols;
    for (auto& col : table_dat.table_desc.columns) {
        if (col.dm_type == "StandardStMan") {
            ssm_cols.push_back(col);
        } else {
            tsm_cols.push_back(col);
        }
    }

    // ---------------------------------------------------------------
    // Write SSM columns (scalars + fixed-shape arrays + indirect arrays).
    // ---------------------------------------------------------------
    {
        SsmWriter ssm;
        ssm.setup(ssm_cols, nrows, false, "StandardStMan");

        for (std::size_t ci = 0; ci < ssm_cols.size(); ++ci) {
            const auto& col = ssm_cols[ci];
            const auto& col_name = col.name;

            for (std::uint64_t r = 0; r < nrows; ++r) {
                const auto& row = main_rows_[r];

                // --- Scalar columns ---
                if (col.kind == ColumnKind::scalar) {
                    CellValue val;
                    if (col_name == "ANTENNA1") { val = row.antenna1; }
                    else if (col_name == "ANTENNA2") { val = row.antenna2; }
                    else if (col_name == "ARRAY_ID") { val = row.array_id; }
                    else if (col_name == "DATA_DESC_ID") { val = row.data_desc_id; }
                    else if (col_name == "EXPOSURE") { val = row.exposure; }
                    else if (col_name == "FEED1") { val = row.feed1; }
                    else if (col_name == "FEED2") { val = row.feed2; }
                    else if (col_name == "FIELD_ID") { val = row.field_id; }
                    else if (col_name == "FLAG_ROW") { val = row.flag_row; }
                    else if (col_name == "INTERVAL") { val = row.interval; }
                    else if (col_name == "OBSERVATION_ID") { val = row.observation_id; }
                    else if (col_name == "PROCESSOR_ID") { val = row.processor_id; }
                    else if (col_name == "SCAN_NUMBER") { val = row.scan_number; }
                    else if (col_name == "STATE_ID") { val = row.state_id; }
                    else if (col_name == "TIME") { val = row.time; }
                    else if (col_name == "TIME_CENTROID") { val = row.time_centroid; }
                    else {
                        if (col.data_type == DataType::tp_int) { val = std::int32_t{0}; }
                        else if (col.data_type == DataType::tp_double) { val = 0.0; }
                        else if (col.data_type == DataType::tp_bool) { val = false; }
                        else { val = std::string{}; }
                    }
                    ssm.write_cell(ci, val, r);
                    continue;
                }

                // --- Array columns in SSM ---
                // Fixed-shape arrays → write_array_*()
                // Variable-shape arrays → write_indirect_array()

                if (col_name == "SIGMA") {
                    // Variable-shape (ndim=1, no fixed shape).
                    const auto ncorr = static_cast<std::int32_t>(row.sigma.size());
                    std::vector<std::int32_t> shape = {ncorr};
                    std::vector<std::uint8_t> raw(row.sigma.size() * 4);
                    auto* p = raw.data();
                    for (const auto v : row.sigma) {
                        std::memcpy(p, &v, 4); // LE (native)
                        p += 4;
                    }
                    ssm.write_indirect_array(ci, shape, raw, r);
                } else if (col_name == "WEIGHT") {
                    const auto ncorr = static_cast<std::int32_t>(row.weight.size());
                    std::vector<std::int32_t> shape = {ncorr};
                    std::vector<std::uint8_t> raw(row.weight.size() * 4);
                    auto* p = raw.data();
                    for (const auto v : row.weight) {
                        std::memcpy(p, &v, 4);
                        p += 4;
                    }
                    ssm.write_indirect_array(ci, shape, raw, r);
                } else if (col_name == "FLAG") {
                    // FLAG: bool array [ncorr, nchan], variable-shape.
                    // Bool arrays in SSMIndColumn are stored as bytes in
                    // the indirect file (1 byte per element).
                    if (row.flag.empty()) {
                        // Write zero-length array.
                        ssm.write_indirect_array(ci, {0, 0}, {}, r);
                    } else {
                        // flag is [ncorr * nchan], shape = [ncorr, nchan].
                        // We don't know ncorr/nchan split, but flag.size()
                        // must match sigma.size() * nchan. Use sigma.size()
                        // for ncorr.
                        const auto ncorr = static_cast<std::int32_t>(row.sigma.size());
                        const auto total = static_cast<std::int32_t>(row.flag.size());
                        const auto nchan = (ncorr > 0) ? total / ncorr : 0;
                        std::vector<std::int32_t> shape = {ncorr, nchan};
                        // casacore stores Bool arrays as uChar in StManArrayFile.
                        std::vector<std::uint8_t> raw(row.flag.size());
                        for (std::size_t i = 0; i < row.flag.size(); ++i) {
                            raw[i] = row.flag[i] ? 1 : 0;
                        }
                        ssm.write_indirect_array(ci, shape, raw, r);
                    }
                } else if (col_name == "FLAG_CATEGORY") {
                    // FLAG_CATEGORY: bool array [ncorr, nchan, ncat], always empty.
                    ssm.write_indirect_array(ci, {0, 0, 0}, {}, r);
                } else if (col_name == "UVW") {
                    // UVW: fixed-shape double array [3], direct in SSM.
                    std::vector<double> vals(3, 0.0);
                    for (std::size_t e = 0; e < 3 && e < row.uvw.size(); ++e) {
                        vals[e] = row.uvw[e];
                    }
                    ssm.write_array_double(ci, vals, r);
                } else if (col_name == "DATA") {
                    // DATA: complex array [ncorr, nchan], variable-shape.
                    if (row.data.empty()) {
                        ssm.write_indirect_array(ci, {0, 0}, {}, r);
                    } else {
                        const auto ncorr = static_cast<std::int32_t>(row.sigma.size());
                        const auto total = static_cast<std::int32_t>(row.data.size());
                        const auto nchan = (ncorr > 0) ? total / ncorr : 0;
                        std::vector<std::int32_t> shape = {ncorr, nchan};
                        // Complex = 2 floats = 8 bytes per element.
                        std::vector<std::uint8_t> raw(row.data.size() * 8);
                        auto* p = raw.data();
                        for (const auto& c : row.data) {
                            float re = c.real();
                            float im = c.imag();
                            std::memcpy(p, &re, 4);
                            std::memcpy(p + 4, &im, 4);
                            p += 8;
                        }
                        ssm.write_indirect_array(ci, shape, raw, r);
                    }
                } else {
                    // Unknown array column — write empty indirect array.
                    std::vector<std::int32_t> shape(static_cast<std::size_t>(col.ndim), 0);
                    ssm.write_indirect_array(ci, shape, {}, r);
                }
            }
        }

        table_dat.storage_managers[0].data_blob = ssm.make_blob();
        ssm.write_file(ms_.path().string(), 0);
        ssm.write_indirect_file(ms_.path().string(), 0);
    }

    // ---------------------------------------------------------------
    // Write TSM array columns (only UVW with fixed shape {3}).
    // ---------------------------------------------------------------
    if (!tsm_cols.empty() && nrows > 0) {
        // All TSM columns should already have FixedShape from schema.
        // Update column_setups shapes if needed.
        for (auto& cs : table_dat.column_setups) {
            for (const auto& tc : tsm_cols) {
                if (cs.column_name == tc.name && !tc.shape.empty()) {
                    cs.has_shape = true;
                    cs.shape = tc.shape;
                }
            }
        }

        TiledStManWriter tsm;
        tsm.setup("TiledColumnStMan", "TiledMS", tsm_cols, nrows);

        for (std::size_t ci = 0; ci < tsm_cols.size(); ++ci) {
            const auto& col = tsm_cols[ci];
            for (std::uint64_t r = 0; r < nrows; ++r) {
                const auto& row = main_rows_[r];
                if (col.name == "UVW") {
                    constexpr std::uint64_t kUvwElems = 3;
                    std::vector<std::uint8_t> raw(kUvwElems * sizeof(double));
                    auto* ptr = reinterpret_cast<double*>(raw.data());
                    for (std::uint64_t e = 0; e < kUvwElems && e < row.uvw.size(); ++e) {
                        ptr[e] = row.uvw[e];
                    }
                    tsm.write_raw_cell(ci, raw, r);
                }
            }
        }

        tsm.write_files(ms_.path().string(), 1);
    }

    // Write updated table.dat.
    auto bytes = serialize_table_dat_full(table_dat);
    std::ofstream out(ms_.path() / "table.dat", std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write main table.dat");
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

void MsWriter::flush() {
    validate_foreign_keys();

    // Flush subtables first (FK targets).
    if (!antenna_rows_.empty()) {
        const auto& st = ms_.subtable("ANTENNA");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(antenna_rows_.size());
        flush_subtable("ANTENNA", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = antenna_rows_[r];
                           const auto& cols = ms_.subtable("ANTENNA").columns();
                           const auto& name = cols[ci].name;
                           if (name == "NAME") { return row.name; }
                           if (name == "STATION") { return row.station; }
                           if (name == "TYPE") { return row.type; }
                           if (name == "MOUNT") { return row.mount; }
                           if (name == "DISH_DIAMETER") { return row.dish_diameter; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           // Array columns: return default (skip in flush_subtable).
                           if (cols[ci].data_type == DataType::tp_double) { return 0.0; }
                           return std::int32_t{0};
                       });
    }

    if (!spw_rows_.empty()) {
        const auto& st = ms_.subtable("SPECTRAL_WINDOW");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(spw_rows_.size());
        flush_subtable("SPECTRAL_WINDOW", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = spw_rows_[r];
                           const auto& cols =
                               ms_.subtable("SPECTRAL_WINDOW").columns();
                           const auto& name = cols[ci].name;
                           if (name == "NUM_CHAN") { return row.num_chan; }
                           if (name == "NAME") { return row.name; }
                           if (name == "REF_FREQUENCY") { return row.ref_frequency; }
                           if (name == "MEAS_FREQ_REF") { return row.meas_freq_ref; }
                           if (name == "TOTAL_BANDWIDTH") { return row.total_bandwidth; }
                           if (name == "NET_SIDEBAND") { return row.net_sideband; }
                           if (name == "IF_CONV_CHAIN") { return row.if_conv_chain; }
                           if (name == "FREQ_GROUP") { return row.freq_group; }
                           if (name == "FREQ_GROUP_NAME") { return row.freq_group_name; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           if (cols[ci].data_type == DataType::tp_double) { return 0.0; }
                           return std::int32_t{0};
                       });
    }

    if (!field_rows_.empty()) {
        const auto& st = ms_.subtable("FIELD");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(field_rows_.size());
        flush_subtable("FIELD", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = field_rows_[r];
                           const auto& cols = ms_.subtable("FIELD").columns();
                           const auto& name = cols[ci].name;
                           if (name == "NAME") { return row.name; }
                           if (name == "CODE") { return row.code; }
                           if (name == "TIME") { return row.time; }
                           if (name == "NUM_POLY") { return row.num_poly; }
                           if (name == "SOURCE_ID") { return row.source_id; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           if (cols[ci].data_type == DataType::tp_double) { return 0.0; }
                           return std::int32_t{0};
                       });
    }

    if (!dd_rows_.empty()) {
        const auto& st = ms_.subtable("DATA_DESCRIPTION");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(dd_rows_.size());
        flush_subtable("DATA_DESCRIPTION", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = dd_rows_[r];
                           const auto& cols =
                               ms_.subtable("DATA_DESCRIPTION").columns();
                           const auto& name = cols[ci].name;
                           if (name == "SPECTRAL_WINDOW_ID") { return row.spectral_window_id; }
                           if (name == "POLARIZATION_ID") { return row.polarization_id; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           return std::int32_t{0};
                       });
    }

    if (!pol_rows_.empty()) {
        const auto& st = ms_.subtable("POLARIZATION");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(pol_rows_.size());
        flush_subtable("POLARIZATION", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = pol_rows_[r];
                           const auto& cols =
                               ms_.subtable("POLARIZATION").columns();
                           const auto& name = cols[ci].name;
                           if (name == "NUM_CORR") { return row.num_corr; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           if (cols[ci].data_type == DataType::tp_int) { return std::int32_t{0}; }
                           return false;
                       });
    }

    if (!obs_rows_.empty()) {
        const auto& st = ms_.subtable("OBSERVATION");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(obs_rows_.size());
        flush_subtable("OBSERVATION", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = obs_rows_[r];
                           const auto& cols =
                               ms_.subtable("OBSERVATION").columns();
                           const auto& name = cols[ci].name;
                           if (name == "TELESCOPE_NAME") { return row.telescope_name; }
                           if (name == "OBSERVER") { return row.observer; }
                           if (name == "PROJECT") { return row.project; }
                           if (name == "RELEASE_DATE") { return row.release_date; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           if (name == "SCHEDULE_TYPE") { return std::string{}; }
                           if (cols[ci].data_type == DataType::tp_double) { return 0.0; }
                           if (cols[ci].data_type == DataType::tp_string) {
                               return std::string{};
                           }
                           return std::int32_t{0};
                       });
    }

    if (!state_rows_.empty()) {
        const auto& st = ms_.subtable("STATE");
        const auto& cols = st.columns();
        const auto nrows = static_cast<std::uint64_t>(state_rows_.size());
        flush_subtable("STATE", cols, nrows,
                       [this](std::size_t ci, std::uint64_t r) -> CellValue {
                           const auto& row = state_rows_[r];
                           const auto& cols = ms_.subtable("STATE").columns();
                           const auto& name = cols[ci].name;
                           if (name == "SIG") { return row.sig; }
                           if (name == "REF") { return row.ref; }
                           if (name == "CAL") { return row.cal; }
                           if (name == "LOAD") { return row.load; }
                           if (name == "SUB_SCAN") { return row.sub_scan; }
                           if (name == "OBS_MODE") { return row.obs_mode; }
                           if (name == "FLAG_ROW") { return row.flag_row; }
                           if (cols[ci].data_type == DataType::tp_double) { return 0.0; }
                           if (cols[ci].data_type == DataType::tp_bool) { return false; }
                           return std::string{};
                       });
    }

    // Flush main table last (references subtable rows).
    flush_main_table();
}

} // namespace casacore_mini
