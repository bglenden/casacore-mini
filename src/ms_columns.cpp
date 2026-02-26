#include "casacore_mini/ms_columns.hpp"

#include "casacore_mini/table_measure_column.hpp"

#include <stdexcept>

namespace casacore_mini {

// ===========================================================================
// MsMainColumns
// ===========================================================================

MsMainColumns::MsMainColumns(MeasurementSet& ms) : table_(ms.main_table()) {}

void MsMainColumns::ensure_measure_descs() const {
    if (measure_descs_loaded_) {
        return;
    }
    measure_descs_loaded_ = true;

    for (const auto& col : table_.columns()) {
        if (col.name == "TIME") {
            time_meas_desc_ = read_table_measure_desc(col.name, col.keywords);
        } else if (col.name == "UVW") {
            uvw_meas_desc_ = read_table_measure_desc(col.name, col.keywords);
        }
    }
}

std::int32_t MsMainColumns::antenna1(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("ANTENNA1", row));
}
std::int32_t MsMainColumns::antenna2(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("ANTENNA2", row));
}
std::int32_t MsMainColumns::array_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("ARRAY_ID", row));
}
std::int32_t MsMainColumns::data_desc_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("DATA_DESC_ID", row));
}
double MsMainColumns::exposure(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("EXPOSURE", row));
}
std::int32_t MsMainColumns::feed1(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("FEED1", row));
}
std::int32_t MsMainColumns::feed2(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("FEED2", row));
}
std::int32_t MsMainColumns::field_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("FIELD_ID", row));
}
bool MsMainColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
double MsMainColumns::interval(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("INTERVAL", row));
}
std::int32_t MsMainColumns::observation_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("OBSERVATION_ID", row));
}
std::int32_t MsMainColumns::processor_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("PROCESSOR_ID", row));
}
std::int32_t MsMainColumns::scan_number(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("SCAN_NUMBER", row));
}
std::int32_t MsMainColumns::state_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("STATE_ID", row));
}
double MsMainColumns::time(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("TIME", row));
}
double MsMainColumns::time_centroid(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("TIME_CENTROID", row));
}

std::vector<double> MsMainColumns::uvw(std::uint64_t row) const {
    return table_.read_array_double_cell("UVW", row);
}

std::vector<float> MsMainColumns::sigma(std::uint64_t row) const {
    return table_.read_array_float_cell("SIGMA", row);
}

std::vector<float> MsMainColumns::weight(std::uint64_t row) const {
    return table_.read_array_float_cell("WEIGHT", row);
}

Measure MsMainColumns::time_measure(std::uint64_t row) const {
    ensure_measure_descs();
    if (!time_meas_desc_.has_value()) {
        throw std::runtime_error("MsMainColumns: no MEASINFO for TIME column");
    }
    ScalarCellReader reader = [this](const std::string& col_name,
                                      std::uint64_t r) -> CellValue {
        return table_.read_scalar_cell(col_name, r);
    };
    return read_scalar_measure(*time_meas_desc_, reader, row);
}

Measure MsMainColumns::uvw_measure(std::uint64_t row) const {
    ensure_measure_descs();
    if (!uvw_meas_desc_.has_value()) {
        throw std::runtime_error("MsMainColumns: no MEASINFO for UVW column");
    }

    ArrayCellReader array_reader = [this](const std::string& col_name,
                                           std::uint64_t r) -> std::vector<double> {
        return table_.read_array_double_cell(col_name, r);
    };
    ScalarCellReader scalar_reader = [this](const std::string& col_name,
                                             std::uint64_t r) -> CellValue {
        return table_.read_scalar_cell(col_name, r);
    };

    auto measures = read_array_measures(*uvw_meas_desc_, array_reader, scalar_reader, row);
    if (measures.empty()) {
        throw std::runtime_error("MsMainColumns: no UVW measure at row");
    }
    return measures[0];
}

std::uint64_t MsMainColumns::row_count() const noexcept { return table_.nrow(); }

// ===========================================================================
// MsAntennaColumns
// ===========================================================================

MsAntennaColumns::MsAntennaColumns(MeasurementSet& ms) : table_(ms.subtable("ANTENNA")) {}

std::string MsAntennaColumns::name(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("NAME", row));
}
std::string MsAntennaColumns::station(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("STATION", row));
}
std::string MsAntennaColumns::type(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("TYPE", row));
}
std::string MsAntennaColumns::mount(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("MOUNT", row));
}
std::vector<double> MsAntennaColumns::position(std::uint64_t row) const {
    (void)row;
    return {};
}
std::vector<double> MsAntennaColumns::offset(std::uint64_t row) const {
    (void)row;
    return {};
}
double MsAntennaColumns::dish_diameter(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("DISH_DIAMETER", row));
}
bool MsAntennaColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsAntennaColumns::row_count() const noexcept {
    return table_.nrow();
}

// ===========================================================================
// MsSpWindowColumns
// ===========================================================================

MsSpWindowColumns::MsSpWindowColumns(MeasurementSet& ms)
    : table_(ms.subtable("SPECTRAL_WINDOW")) {}

std::int32_t MsSpWindowColumns::num_chan(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("NUM_CHAN", row));
}
std::string MsSpWindowColumns::name(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("NAME", row));
}
double MsSpWindowColumns::ref_frequency(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("REF_FREQUENCY", row));
}
std::int32_t MsSpWindowColumns::meas_freq_ref(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("MEAS_FREQ_REF", row));
}
double MsSpWindowColumns::total_bandwidth(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("TOTAL_BANDWIDTH", row));
}
std::int32_t MsSpWindowColumns::net_sideband(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("NET_SIDEBAND", row));
}
bool MsSpWindowColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsSpWindowColumns::row_count() const noexcept {
    return table_.nrow();
}

// ===========================================================================
// MsFieldColumns
// ===========================================================================

MsFieldColumns::MsFieldColumns(MeasurementSet& ms) : table_(ms.subtable("FIELD")) {}

std::string MsFieldColumns::name(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("NAME", row));
}
std::string MsFieldColumns::code(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("CODE", row));
}
double MsFieldColumns::time(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("TIME", row));
}
std::int32_t MsFieldColumns::num_poly(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("NUM_POLY", row));
}
std::int32_t MsFieldColumns::source_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("SOURCE_ID", row));
}
bool MsFieldColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsFieldColumns::row_count() const noexcept {
    return table_.nrow();
}

// ===========================================================================
// MsDataDescColumns
// ===========================================================================

MsDataDescColumns::MsDataDescColumns(MeasurementSet& ms)
    : table_(ms.subtable("DATA_DESCRIPTION")) {}

std::int32_t MsDataDescColumns::spectral_window_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("SPECTRAL_WINDOW_ID", row));
}
std::int32_t MsDataDescColumns::polarization_id(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("POLARIZATION_ID", row));
}
bool MsDataDescColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsDataDescColumns::row_count() const noexcept {
    return table_.nrow();
}

// ===========================================================================
// MsPolarizationColumns
// ===========================================================================

MsPolarizationColumns::MsPolarizationColumns(MeasurementSet& ms)
    : table_(ms.subtable("POLARIZATION")) {}

std::int32_t MsPolarizationColumns::num_corr(std::uint64_t row) const {
    return std::get<std::int32_t>(table_.read_scalar_cell("NUM_CORR", row));
}
bool MsPolarizationColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsPolarizationColumns::row_count() const noexcept {
    return table_.nrow();
}

// ===========================================================================
// MsObservationColumns
// ===========================================================================

MsObservationColumns::MsObservationColumns(MeasurementSet& ms)
    : table_(ms.subtable("OBSERVATION")) {}

std::string MsObservationColumns::telescope_name(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("TELESCOPE_NAME", row));
}
std::string MsObservationColumns::observer(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("OBSERVER", row));
}
std::string MsObservationColumns::project(std::uint64_t row) const {
    return std::get<std::string>(table_.read_scalar_cell("PROJECT", row));
}
double MsObservationColumns::release_date(std::uint64_t row) const {
    return std::get<double>(table_.read_scalar_cell("RELEASE_DATE", row));
}
bool MsObservationColumns::flag_row(std::uint64_t row) const {
    return std::get<bool>(table_.read_scalar_cell("FLAG_ROW", row));
}
std::uint64_t MsObservationColumns::row_count() const noexcept {
    return table_.nrow();
}

} // namespace casacore_mini
