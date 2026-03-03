// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/ms_subtables.hpp"

#include <array>
#include <string>

namespace casacore_mini {

namespace {

/// Build the casacore-compatible AipsIO type string for a column descriptor.
[[nodiscard]] std::string col_type_string(ColumnKind kind, DataType dtype) {
    const char* type_id = nullptr;
    switch (dtype) {
    case DataType::tp_bool:
        type_id = "Bool    ";
        break;
    case DataType::tp_char:
        type_id = "Char    ";
        break;
    case DataType::tp_uchar:
        type_id = "uChar   ";
        break;
    case DataType::tp_short:
        type_id = "Short   ";
        break;
    case DataType::tp_ushort:
        type_id = "uShort  ";
        break;
    case DataType::tp_int:
        type_id = "Int     ";
        break;
    case DataType::tp_uint:
        type_id = "uInt    ";
        break;
    case DataType::tp_int64:
        type_id = "Int64   ";
        break;
    case DataType::tp_float:
        type_id = "float   ";
        break;
    case DataType::tp_double:
        type_id = "double  ";
        break;
    case DataType::tp_complex:
        type_id = "Complex ";
        break;
    case DataType::tp_dcomplex:
        type_id = "DComplex";
        break;
    case DataType::tp_string:
        type_id = "String  ";
        break;
    default:
        type_id = "unknown ";
        break;
    }
    std::string prefix = (kind == ColumnKind::scalar) ? "ScalarColumnDesc<" : "ArrayColumnDesc<";
    return prefix + type_id;
}

[[nodiscard]] ColumnDesc make_scalar(const std::string& col_name, DataType dt,
                                     const std::string& comment = "") {
    ColumnDesc col;
    col.kind = ColumnKind::scalar;
    col.name = col_name;
    col.data_type = dt;
    col.dm_type = "StandardStMan";
    col.dm_group = "StandardStMan";
    col.type_string = col_type_string(ColumnKind::scalar, dt);
    col.version = 1;
    col.comment = comment;
    return col;
}

[[nodiscard]] ColumnDesc make_array(const std::string& col_name, DataType dt, std::int32_t ndim,
                                    std::vector<std::int64_t> shape = {},
                                    const std::string& comment = "") {
    ColumnDesc col;
    col.kind = ColumnKind::array;
    col.name = col_name;
    col.data_type = dt;
    col.dm_type = "StandardStMan";
    col.dm_group = "StandardStMan";
    col.type_string = col_type_string(ColumnKind::array, dt);
    col.version = 1;
    col.ndim = ndim;
    col.shape = std::move(shape);
    col.comment = comment;
    if (!col.shape.empty()) {
        col.options = 4; // FixedShape
    }
    return col;
}

} // namespace

// ---------------------------------------------------------------------------
// Column name lookup functions
// ---------------------------------------------------------------------------

std::string_view ms_antenna_column_name(MsAntennaColumn col) {
    static constexpr std::array<const char*, 8> kNames = {
        "NAME", "STATION", "TYPE", "MOUNT", "POSITION", "OFFSET", "DISH_DIAMETER", "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_data_desc_column_name(MsDataDescColumn col) {
    static constexpr std::array<const char*, 3> kNames = {"SPECTRAL_WINDOW_ID", "POLARIZATION_ID",
                                                          "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_feed_column_name(MsFeedColumn col) {
    static constexpr std::array<const char*, 12> kNames = {
        "ANTENNA_ID",   "FEED_ID",     "SPECTRAL_WINDOW_ID",
        "TIME",         "INTERVAL",    "NUM_RECEPTORS",
        "BEAM_ID",      "BEAM_OFFSET", "POLARIZATION_TYPE",
        "POL_RESPONSE", "POSITION",    "RECEPTOR_ANGLE"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_field_column_name(MsFieldColumn col) {
    static constexpr std::array<const char*, 9> kNames = {"NAME",          "CODE",      "TIME",
                                                          "NUM_POLY",      "DELAY_DIR", "PHASE_DIR",
                                                          "REFERENCE_DIR", "SOURCE_ID", "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_flag_cmd_column_name(MsFlagCmdColumn col) {
    static constexpr std::array<const char*, 8> kNames = {
        "TIME", "INTERVAL", "TYPE", "REASON", "LEVEL", "SEVERITY", "APPLIED", "COMMAND"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_history_column_name(MsHistoryColumn col) {
    static constexpr std::array<const char*, 9> kNames = {
        "TIME",      "OBSERVATION_ID", "MESSAGE",     "PRIORITY",  "ORIGIN",
        "OBJECT_ID", "APPLICATION",    "CLI_COMMAND", "APP_PARAMS"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_observation_column_name(MsObservationColumn col) {
    static constexpr std::array<const char*, 9> kNames = {
        "TELESCOPE_NAME", "TIME_RANGE", "OBSERVER",     "LOG",     "SCHEDULE_TYPE",
        "SCHEDULE",       "PROJECT",    "RELEASE_DATE", "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_pointing_column_name(MsPointingColumn col) {
    static constexpr std::array<const char*, 9> kNames = {"ANTENNA_ID", "TIME",     "INTERVAL",
                                                          "NAME",       "NUM_POLY", "TIME_ORIGIN",
                                                          "DIRECTION",  "TARGET",   "TRACKING"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_polarization_column_name(MsPolarizationColumn col) {
    static constexpr std::array<const char*, 4> kNames = {"NUM_CORR", "CORR_TYPE", "CORR_PRODUCT",
                                                          "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_processor_column_name(MsProcessorColumn col) {
    static constexpr std::array<const char*, 5> kNames = {"TYPE", "SUB_TYPE", "TYPE_ID", "MODE_ID",
                                                          "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_spw_column_name(MsSpWindowColumn col) {
    static constexpr std::array<const char*, 14> kNames = {
        "NUM_CHAN",      "NAME",       "REF_FREQUENCY",   "CHAN_FREQ",       "CHAN_WIDTH",
        "EFFECTIVE_BW",  "RESOLUTION", "MEAS_FREQ_REF",   "TOTAL_BANDWIDTH", "NET_SIDEBAND",
        "IF_CONV_CHAIN", "FREQ_GROUP", "FREQ_GROUP_NAME", "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_state_column_name(MsStateColumn col) {
    static constexpr std::array<const char*, 7> kNames = {"SIG",      "REF",      "CAL",     "LOAD",
                                                          "SUB_SCAN", "OBS_MODE", "FLAG_ROW"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_doppler_column_name(MsDopplerColumn col) {
    static constexpr std::array<const char*, 4> kNames = {"DOPPLER_ID", "SOURCE_ID",
                                                          "TRANSITION_ID", "VELDEF"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_freq_offset_column_name(MsFreqOffsetColumn col) {
    static constexpr std::array<const char*, 7> kNames = {
        "ANTENNA1", "ANTENNA2", "FEED_ID", "SPECTRAL_WINDOW_ID", "TIME", "INTERVAL", "OFFSET"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_source_column_name(MsSourceColumn col) {
    static constexpr std::array<const char*, 14> kNames = {
        "SOURCE_ID",      "TIME",     "INTERVAL",          "SPECTRAL_WINDOW_ID",
        "NUM_LINES",      "NAME",     "CALIBRATION_GROUP", "CODE",
        "DIRECTION",      "POSITION", "PROPER_MOTION",     "TRANSITION",
        "REST_FREQUENCY", "SYSVEL"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_syscal_column_name(MsSysCalColumn col) {
    static constexpr std::array<const char*, 9> kNames = {
        "ANTENNA_ID", "FEED_ID", "SPECTRAL_WINDOW_ID", "TIME", "INTERVAL", "TSYS",
        "TRX",        "TSKY",    "PHASE_DIFF"};
    return kNames.at(static_cast<std::size_t>(col));
}

std::string_view ms_weather_column_name(MsWeatherColumn col) {
    static constexpr std::array<const char*, 9> kNames = {
        "ANTENNA_ID",   "TIME",      "INTERVAL",       "TEMPERATURE", "PRESSURE",
        "REL_HUMIDITY", "DEW_POINT", "WIND_DIRECTION", "WIND_SPEED"};
    return kNames.at(static_cast<std::size_t>(col));
}

// ---------------------------------------------------------------------------
// Schema factory functions
// ---------------------------------------------------------------------------

std::vector<ColumnDesc> make_antenna_desc() {
    return {
        make_scalar("NAME", DataType::tp_string, "Antenna name"),
        make_scalar("STATION", DataType::tp_string, "Station name"),
        make_scalar("TYPE", DataType::tp_string, "Antenna type"),
        make_scalar("MOUNT", DataType::tp_string, "Mount type"),
        make_array("POSITION", DataType::tp_double, 1, {3}, "Antenna position in ITRF"),
        make_array("OFFSET", DataType::tp_double, 1, {3}, "Axes offset"),
        make_scalar("DISH_DIAMETER", DataType::tp_double, "Physical dish diameter"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_data_description_desc() {
    return {
        make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "Spectral window ID"),
        make_scalar("POLARIZATION_ID", DataType::tp_int, "Polarization setup ID"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_feed_desc() {
    return {
        make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
        make_scalar("FEED_ID", DataType::tp_int, "Feed ID"),
        make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "Spectral window ID"),
        make_scalar("TIME", DataType::tp_double, "Midpoint of time interval"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("NUM_RECEPTORS", DataType::tp_int, "Number of receptors"),
        make_array("BEAM_ID", DataType::tp_int, 1, {}, "Beam ID"),
        make_array("BEAM_OFFSET", DataType::tp_double, 2, {}, "Beam position offset"),
        make_array("POLARIZATION_TYPE", DataType::tp_string, 1, {}, "Polarization type"),
        make_array("POL_RESPONSE", DataType::tp_complex, 2, {}, "Polarization response"),
        make_array("POSITION", DataType::tp_double, 1, {3}, "Feed position"),
        make_array("RECEPTOR_ANGLE", DataType::tp_double, 1, {}, "Receptor angle"),
    };
}

std::vector<ColumnDesc> make_field_desc() {
    return {
        make_scalar("NAME", DataType::tp_string, "Field name"),
        make_scalar("CODE", DataType::tp_string, "Field code"),
        make_scalar("TIME", DataType::tp_double, "Time origin for polynomials"),
        make_scalar("NUM_POLY", DataType::tp_int, "Polynomial order"),
        make_array("DELAY_DIR", DataType::tp_double, 2, {}, "Delay tracking direction"),
        make_array("PHASE_DIR", DataType::tp_double, 2, {}, "Phase tracking direction"),
        make_array("REFERENCE_DIR", DataType::tp_double, 2, {}, "Reference direction"),
        make_scalar("SOURCE_ID", DataType::tp_int, "Source ID"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_flag_cmd_desc() {
    return {
        make_scalar("TIME", DataType::tp_double, "Midpoint of time interval"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("TYPE", DataType::tp_string, "Flag type"),
        make_scalar("REASON", DataType::tp_string, "Flag reason"),
        make_scalar("LEVEL", DataType::tp_int, "Flag level"),
        make_scalar("SEVERITY", DataType::tp_int, "Severity code"),
        make_scalar("APPLIED", DataType::tp_bool, "Applied flag"),
        make_scalar("COMMAND", DataType::tp_string, "Flag command string"),
    };
}

std::vector<ColumnDesc> make_history_desc() {
    return {
        make_scalar("TIME", DataType::tp_double, "Timestamp"),
        make_scalar("OBSERVATION_ID", DataType::tp_int, "Observation ID"),
        make_scalar("MESSAGE", DataType::tp_string, "Log message"),
        make_scalar("PRIORITY", DataType::tp_string, "Priority level"),
        make_scalar("ORIGIN", DataType::tp_string, "Origin of log entry"),
        make_scalar("OBJECT_ID", DataType::tp_int, "Object ID"),
        make_scalar("APPLICATION", DataType::tp_string, "Application name"),
        make_array("CLI_COMMAND", DataType::tp_string, 1, {}, "CLI command history"),
        make_array("APP_PARAMS", DataType::tp_string, 1, {}, "Application parameters"),
    };
}

std::vector<ColumnDesc> make_observation_desc() {
    return {
        make_scalar("TELESCOPE_NAME", DataType::tp_string, "Telescope name"),
        make_array("TIME_RANGE", DataType::tp_double, 1, {2}, "Start and end of observation"),
        make_scalar("OBSERVER", DataType::tp_string, "Observer name"),
        make_array("LOG", DataType::tp_string, 1, {}, "Observing log"),
        make_scalar("SCHEDULE_TYPE", DataType::tp_string, "Schedule type"),
        make_array("SCHEDULE", DataType::tp_string, 1, {}, "Schedule entries"),
        make_scalar("PROJECT", DataType::tp_string, "Project ID"),
        make_scalar("RELEASE_DATE", DataType::tp_double, "Release date (MJD)"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_pointing_desc() {
    return {
        make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
        make_scalar("TIME", DataType::tp_double, "Time of measurement"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("NAME", DataType::tp_string, "Pointing model name"),
        make_scalar("NUM_POLY", DataType::tp_int, "Polynomial order"),
        make_scalar("TIME_ORIGIN", DataType::tp_double, "Time origin for polynomials"),
        make_array("DIRECTION", DataType::tp_double, 2, {}, "Antenna pointing direction"),
        make_array("TARGET", DataType::tp_double, 2, {}, "Target direction"),
        make_scalar("TRACKING", DataType::tp_bool, "Tracking flag"),
    };
}

std::vector<ColumnDesc> make_polarization_desc() {
    return {
        make_scalar("NUM_CORR", DataType::tp_int, "Number of correlations"),
        make_array("CORR_TYPE", DataType::tp_int, 1, {}, "Correlation type codes"),
        make_array("CORR_PRODUCT", DataType::tp_int, 2, {}, "Correlation product indices"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_processor_desc() {
    return {
        make_scalar("TYPE", DataType::tp_string, "Processor type"),
        make_scalar("SUB_TYPE", DataType::tp_string, "Processor sub-type"),
        make_scalar("TYPE_ID", DataType::tp_int, "Type ID"),
        make_scalar("MODE_ID", DataType::tp_int, "Mode ID"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_spectral_window_desc() {
    return {
        make_scalar("NUM_CHAN", DataType::tp_int, "Number of channels"),
        make_scalar("NAME", DataType::tp_string, "Spectral window name"),
        make_scalar("REF_FREQUENCY", DataType::tp_double, "Reference frequency (Hz)"),
        make_array("CHAN_FREQ", DataType::tp_double, 1, {}, "Channel frequencies (Hz)"),
        make_array("CHAN_WIDTH", DataType::tp_double, 1, {}, "Channel widths (Hz)"),
        make_array("EFFECTIVE_BW", DataType::tp_double, 1, {}, "Effective bandwidths (Hz)"),
        make_array("RESOLUTION", DataType::tp_double, 1, {}, "Spectral resolution (Hz)"),
        make_scalar("MEAS_FREQ_REF", DataType::tp_int, "Frequency measure reference"),
        make_scalar("TOTAL_BANDWIDTH", DataType::tp_double, "Total bandwidth (Hz)"),
        make_scalar("NET_SIDEBAND", DataType::tp_int, "Net sideband"),
        make_scalar("IF_CONV_CHAIN", DataType::tp_int, "IF conversion chain"),
        make_scalar("FREQ_GROUP", DataType::tp_int, "Frequency group"),
        make_scalar("FREQ_GROUP_NAME", DataType::tp_string, "Frequency group name"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_state_desc() {
    return {
        make_scalar("SIG", DataType::tp_bool, "Signal flag"),
        make_scalar("REF", DataType::tp_bool, "Reference flag"),
        make_scalar("CAL", DataType::tp_double, "Calibration noise"),
        make_scalar("LOAD", DataType::tp_double, "Load temperature"),
        make_scalar("SUB_SCAN", DataType::tp_int, "Sub-scan number"),
        make_scalar("OBS_MODE", DataType::tp_string, "Observing mode"),
        make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
    };
}

std::vector<ColumnDesc> make_doppler_desc() {
    return {
        make_scalar("DOPPLER_ID", DataType::tp_int, "Doppler tracking ID"),
        make_scalar("SOURCE_ID", DataType::tp_int, "Source ID"),
        make_scalar("TRANSITION_ID", DataType::tp_int, "Transition ID"),
        make_scalar("VELDEF", DataType::tp_double, "Velocity definition (m/s)"),
    };
}

std::vector<ColumnDesc> make_freq_offset_desc() {
    return {
        make_scalar("ANTENNA1", DataType::tp_int, "First antenna ID"),
        make_scalar("ANTENNA2", DataType::tp_int, "Second antenna ID"),
        make_scalar("FEED_ID", DataType::tp_int, "Feed ID"),
        make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "Spectral window ID"),
        make_scalar("TIME", DataType::tp_double, "Time of measurement"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("OFFSET", DataType::tp_double, "Frequency offset (Hz)"),
    };
}

std::vector<ColumnDesc> make_source_desc() {
    return {
        make_scalar("SOURCE_ID", DataType::tp_int, "Source ID"),
        make_scalar("TIME", DataType::tp_double, "Midpoint of time interval"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "Spectral window ID"),
        make_scalar("NUM_LINES", DataType::tp_int, "Number of spectral lines"),
        make_scalar("NAME", DataType::tp_string, "Source name"),
        make_scalar("CALIBRATION_GROUP", DataType::tp_int, "Calibration group"),
        make_scalar("CODE", DataType::tp_string, "Source code"),
        make_array("DIRECTION", DataType::tp_double, 1, {2}, "Source direction (rad)"),
        make_array("POSITION", DataType::tp_double, 1, {3}, "Source position (m)"),
        make_array("PROPER_MOTION", DataType::tp_double, 1, {2}, "Proper motion (rad/s)"),
        make_array("TRANSITION", DataType::tp_string, 1, {}, "Spectral line transition"),
        make_array("REST_FREQUENCY", DataType::tp_double, 1, {}, "Rest frequencies (Hz)"),
        make_array("SYSVEL", DataType::tp_double, 1, {}, "Systemic velocity (m/s)"),
    };
}

std::vector<ColumnDesc> make_syscal_desc() {
    return {
        make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
        make_scalar("FEED_ID", DataType::tp_int, "Feed ID"),
        make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "Spectral window ID"),
        make_scalar("TIME", DataType::tp_double, "Time of measurement"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_array("TSYS", DataType::tp_float, 1, {}, "System temperature"),
        make_array("TRX", DataType::tp_float, 1, {}, "Receiver temperature"),
        make_array("TSKY", DataType::tp_float, 1, {}, "Sky temperature"),
        make_scalar("PHASE_DIFF", DataType::tp_float, "Phase difference"),
    };
}

std::vector<ColumnDesc> make_weather_desc() {
    return {
        make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
        make_scalar("TIME", DataType::tp_double, "Time of measurement"),
        make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
        make_scalar("TEMPERATURE", DataType::tp_float, "Temperature (K)"),
        make_scalar("PRESSURE", DataType::tp_float, "Pressure (Pa)"),
        make_scalar("REL_HUMIDITY", DataType::tp_float, "Relative humidity"),
        make_scalar("DEW_POINT", DataType::tp_float, "Dew point (K)"),
        make_scalar("WIND_DIRECTION", DataType::tp_float, "Wind direction (rad)"),
        make_scalar("WIND_SPEED", DataType::tp_float, "Wind speed (m/s)"),
    };
}

std::vector<ColumnDesc> make_subtable_desc_by_name(std::string_view name) {
    if (name == "ANTENNA") {
        return make_antenna_desc();
    }
    if (name == "DATA_DESCRIPTION") {
        return make_data_description_desc();
    }
    if (name == "FEED") {
        return make_feed_desc();
    }
    if (name == "FIELD") {
        return make_field_desc();
    }
    if (name == "FLAG_CMD") {
        return make_flag_cmd_desc();
    }
    if (name == "HISTORY") {
        return make_history_desc();
    }
    if (name == "OBSERVATION") {
        return make_observation_desc();
    }
    if (name == "POINTING") {
        return make_pointing_desc();
    }
    if (name == "POLARIZATION") {
        return make_polarization_desc();
    }
    if (name == "PROCESSOR") {
        return make_processor_desc();
    }
    if (name == "SPECTRAL_WINDOW") {
        return make_spectral_window_desc();
    }
    if (name == "STATE") {
        return make_state_desc();
    }
    if (name == "DOPPLER") {
        return make_doppler_desc();
    }
    if (name == "FREQ_OFFSET") {
        return make_freq_offset_desc();
    }
    if (name == "SOURCE") {
        return make_source_desc();
    }
    if (name == "SYSCAL") {
        return make_syscal_desc();
    }
    if (name == "WEATHER") {
        return make_weather_desc();
    }
    return {};
}

} // namespace casacore_mini
