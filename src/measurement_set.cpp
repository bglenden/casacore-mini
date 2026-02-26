#include "casacore_mini/measurement_set.hpp"

#include "casacore_mini/ms_enums.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_desc_writer.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

/// Build the casacore-compatible AipsIO type string for a column descriptor.
/// Format: "ScalarColumnDesc<float   " or "ArrayColumnDesc<Int     " (no closing >).
[[nodiscard]] std::string col_type_string(ColumnKind kind, DataType dtype) {
    const char* type_id = nullptr;
    switch (dtype) {
    case DataType::tp_bool: type_id = "Bool    "; break;
    case DataType::tp_char: type_id = "Char    "; break;
    case DataType::tp_uchar: type_id = "uChar   "; break;
    case DataType::tp_short: type_id = "Short   "; break;
    case DataType::tp_ushort: type_id = "uShort  "; break;
    case DataType::tp_int: type_id = "Int     "; break;
    case DataType::tp_uint: type_id = "uInt    "; break;
    case DataType::tp_int64: type_id = "Int64   "; break;
    case DataType::tp_float: type_id = "float   "; break;
    case DataType::tp_double: type_id = "double  "; break;
    case DataType::tp_complex: type_id = "Complex "; break;
    case DataType::tp_dcomplex: type_id = "DComplex"; break;
    case DataType::tp_string: type_id = "String  "; break;
    default: type_id = "unknown "; break;
    }
    std::string prefix =
        (kind == ColumnKind::scalar) ? "ScalarColumnDesc<" : "ArrayColumnDesc<";
    return prefix + type_id;
}

/// Convert MsColumnInfo to a ColumnDesc for table.dat.
[[nodiscard]] ColumnDesc to_column_desc(const MsColumnInfo& info,
                                         std::string_view dm_type,
                                         std::string_view dm_group) {
    ColumnDesc col;
    col.kind = info.kind;
    col.name = info.name;
    col.data_type = info.data_type;
    col.dm_type = std::string(dm_type);
    col.dm_group = std::string(dm_group);
    col.type_string = col_type_string(info.kind, info.data_type);
    col.version = 1;
    col.ndim = info.ndim;
    col.shape = info.shape;
    col.comment = info.comment;
    if (info.kind == ColumnKind::array && info.ndim > 0) {
        if (!info.shape.empty()) {
            // Fixed-shape array: Direct (1) | FixedShape (4) = 5.
            // Direct means stored inline in the SSM bucket (SSMDirColumn).
            col.options = 5;
        } else {
            // Variable-shape array: no flags (SSMIndColumn via .f0i).
            col.options = 0;
        }
    }
    return col;
}

/// Add MEASINFO keywords to a column for TIME (MEpoch, UTC, "s").
void add_time_measure_keywords(ColumnDesc& col) {
    Record measinfo;
    measinfo.set("type", RecordValue(std::string("epoch")));
    measinfo.set("Ref", RecordValue(std::string("UTC")));
    col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    RecordArray<std::string> units;
    units.shape = {1};
    units.elements = {"s"};
    col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
}

/// Add MEASINFO keywords to a column for UVW (Muvw, J2000, "m").
void add_uvw_measure_keywords(ColumnDesc& col) {
    Record measinfo;
    measinfo.set("type", RecordValue(std::string("uvw")));
    measinfo.set("Ref", RecordValue(std::string("J2000")));
    col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    RecordArray<std::string> units;
    units.shape = {3};
    units.elements = {"m", "m", "m"};
    col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
}

/// Add MEASINFO keywords to a column for EXPOSURE/INTERVAL (units only, "s").
void add_time_quantum_keywords(ColumnDesc& col) {
    RecordArray<std::string> units;
    units.shape = {1};
    units.elements = {"s"};
    col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
}

/// Check if a column list contains any variable-shape array columns.
[[nodiscard]] bool has_indirect_array_columns(const std::vector<ColumnDesc>& columns) {
    for (const auto& col : columns) {
        if (col.kind == ColumnKind::array && col.shape.empty()) {
            return true;
        }
    }
    return false;
}

/// Write an empty StManArrayFile (.f0i) for indirect array storage.
/// Format: version(uInt) + length(Int64) + padding(Int), all in little-endian.
void write_empty_array_file(const std::filesystem::path& dir, std::uint32_t seq_nr) {
    const auto path = dir / ("table.f" + std::to_string(seq_nr) + "i");
    std::vector<std::uint8_t> buf(16, 0);
    // version = 1 (LE)
    buf[0] = 1;
    // leng = 16 (LE Int64)
    buf[4] = 16;
    // padding = 0 (already zero)
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(buf.data()),
              static_cast<std::streamsize>(buf.size()));
}

/// Create an empty subtable directory with the given columns.
void create_empty_subtable(const std::filesystem::path& dir,
                            const std::vector<ColumnDesc>& columns) {
    auto table_dat = make_subtable_dat(columns);

    std::filesystem::create_directories(dir);

    // Write table.dat.
    auto bytes = serialize_table_dat_full(table_dat);
    std::ofstream out(dir / "table.dat", std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write: " + (dir / "table.dat").string());
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));

    // Write empty .f0 file for the SSM.
    SsmWriter ssm;
    ssm.setup(columns, 0, false, "StandardStMan");
    ssm.write_file(dir.string(), 0);

    // Write empty .f0i file if any variable-shape array columns exist.
    if (has_indirect_array_columns(columns)) {
        write_empty_array_file(dir, 0);
    }

    // Write table.info — casacore subtables have empty type/subtype.
    write_table_info(dir, "", "");
}

/// Build minimal column descriptors for a required subtable.
/// Returns empty vector for unknown subtable names (optional subtables
/// get full schemas in Wave 3).
[[nodiscard]] std::vector<ColumnDesc> make_required_subtable_columns(std::string_view name) {
    // Minimal schemas for each required subtable (key columns only).
    // Full schemas with all columns are defined in Wave 3 (ms_subtables.hpp).
    auto make_scalar = [](const std::string& col_name, DataType dt,
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
    };

    auto make_array = [](const std::string& col_name, DataType dt,
                          std::int32_t ndim, std::vector<std::int64_t> shape = {},
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
            col.options = 5; // Direct (1) | FixedShape (4)
        }
        return col;
    };

    // Helpers to add required keyword metadata.
    auto add_unit_keyword = [](ColumnDesc& col, const std::string& unit) {
        RecordArray<std::string> units;
        units.shape = {1};
        units.elements = {unit};
        col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
    };
    auto add_position_keywords = [](ColumnDesc& col) {
        Record measinfo;
        measinfo.set("type", RecordValue(std::string("position")));
        measinfo.set("Ref", RecordValue(std::string("ITRF")));
        col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));
        RecordArray<std::string> units;
        units.shape = {3};
        units.elements = {"m", "m", "m"};
        col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
    };
    auto add_direction_keywords = [](ColumnDesc& col) {
        Record measinfo;
        measinfo.set("type", RecordValue(std::string("direction")));
        measinfo.set("Ref", RecordValue(std::string("J2000")));
        col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));
        RecordArray<std::string> units;
        units.shape = {2};
        units.elements = {"rad", "rad"};
        col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
    };
    auto add_epoch_keywords = [](ColumnDesc& col) {
        Record measinfo;
        measinfo.set("type", RecordValue(std::string("epoch")));
        measinfo.set("Ref", RecordValue(std::string("UTC")));
        col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));
        RecordArray<std::string> units;
        units.shape = {1};
        units.elements = {"s"};
        col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
    };
    auto add_frequency_keywords = [](ColumnDesc& col) {
        Record measinfo;
        measinfo.set("type", RecordValue(std::string("frequency")));
        measinfo.set("Ref", RecordValue(std::string("TOPO")));
        col.keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));
        RecordArray<std::string> units;
        units.shape = {1};
        units.elements = {"Hz"};
        col.keywords.set("QuantumUnits", RecordValue(std::move(units)));
    };

    if (name == "ANTENNA") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("NAME", DataType::tp_string, "Antenna name"),
            make_scalar("STATION", DataType::tp_string, "Station name"),
            make_scalar("TYPE", DataType::tp_string, "Antenna type"),
            make_scalar("MOUNT", DataType::tp_string, "Mount type"),
            make_array("POSITION", DataType::tp_double, 1, {3}, "Antenna position"),
            make_array("OFFSET", DataType::tp_double, 1, {3}, "Axes offset"),
            make_scalar("DISH_DIAMETER", DataType::tp_double, "Dish diameter"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
        // Add required keywords for POSITION, OFFSET (position measure), DISH_DIAMETER (unit).
        for (auto& col : cols) {
            if (col.name == "POSITION" || col.name == "OFFSET") {
                add_position_keywords(col);
            } else if (col.name == "DISH_DIAMETER") {
                add_unit_keyword(col, "m");
            }
        }
        return cols;
    }
    if (name == "DATA_DESCRIPTION") {
        return {
            make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "SPW ID"),
            make_scalar("POLARIZATION_ID", DataType::tp_int, "Polarization ID"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
    }
    if (name == "FEED") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
            make_scalar("FEED_ID", DataType::tp_int, "Feed ID"),
            make_scalar("SPECTRAL_WINDOW_ID", DataType::tp_int, "SPW ID"),
            make_scalar("TIME", DataType::tp_double, "Midpoint time"),
            make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
            make_scalar("NUM_RECEPTORS", DataType::tp_int, "Number of receptors"),
            make_scalar("BEAM_ID", DataType::tp_int, "Beam ID"),
            make_array("BEAM_OFFSET", DataType::tp_double, 2, {}, "Beam offset"),
            make_array("POLARIZATION_TYPE", DataType::tp_string, 1, {}, "Polarization type"),
            make_array("POL_RESPONSE", DataType::tp_complex, 2, {}, "Polarization response"),
            make_array("POSITION", DataType::tp_double, 1, {3}, "Position"),
            make_array("RECEPTOR_ANGLE", DataType::tp_double, 1, {}, "Receptor angle"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME") { add_epoch_keywords(col); }
            else if (col.name == "INTERVAL") { add_unit_keyword(col, "s"); }
            else if (col.name == "BEAM_OFFSET") { add_direction_keywords(col); }
            else if (col.name == "POSITION") { add_position_keywords(col); }
            else if (col.name == "RECEPTOR_ANGLE") { add_unit_keyword(col, "rad"); }
        }
        return cols;
    }
    if (name == "FIELD") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("NAME", DataType::tp_string, "Field name"),
            make_scalar("CODE", DataType::tp_string, "Field code"),
            make_scalar("TIME", DataType::tp_double, "Time origin"),
            make_scalar("NUM_POLY", DataType::tp_int, "Number of polynomials"),
            make_array("DELAY_DIR", DataType::tp_double, 2, {}, "Delay direction"),
            make_array("PHASE_DIR", DataType::tp_double, 2, {}, "Phase direction"),
            make_array("REFERENCE_DIR", DataType::tp_double, 2, {}, "Reference direction"),
            make_scalar("SOURCE_ID", DataType::tp_int, "Source ID"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME") { add_epoch_keywords(col); }
            else if (col.name == "DELAY_DIR" || col.name == "PHASE_DIR" ||
                     col.name == "REFERENCE_DIR") { add_direction_keywords(col); }
        }
        return cols;
    }
    if (name == "FLAG_CMD") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("TIME", DataType::tp_double, "Midpoint time"),
            make_scalar("INTERVAL", DataType::tp_double, "Time interval"),
            make_scalar("TYPE", DataType::tp_string, "Flag type"),
            make_scalar("REASON", DataType::tp_string, "Flag reason"),
            make_scalar("LEVEL", DataType::tp_int, "Flag level"),
            make_scalar("SEVERITY", DataType::tp_int, "Severity code"),
            make_scalar("APPLIED", DataType::tp_bool, "Applied flag"),
            make_scalar("COMMAND", DataType::tp_string, "Flag command"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME") { add_epoch_keywords(col); }
            else if (col.name == "INTERVAL") { add_unit_keyword(col, "s"); }
        }
        return cols;
    }
    if (name == "HISTORY") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("TIME", DataType::tp_double, "Timestamp"),
            make_scalar("OBSERVATION_ID", DataType::tp_int, "Observation ID"),
            make_scalar("MESSAGE", DataType::tp_string, "Log message"),
            make_scalar("PRIORITY", DataType::tp_string, "Priority level"),
            make_scalar("ORIGIN", DataType::tp_string, "Origin of message"),
            make_scalar("OBJECT_ID", DataType::tp_int, "Object ID"),
            make_scalar("APPLICATION", DataType::tp_string, "Application name"),
            make_array("CLI_COMMAND", DataType::tp_string, 1, {}, "CLI command"),
            make_array("APP_PARAMS", DataType::tp_string, 1, {}, "Application parameters"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME") { add_epoch_keywords(col); }
        }
        return cols;
    }
    if (name == "OBSERVATION") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("TELESCOPE_NAME", DataType::tp_string, "Telescope name"),
            make_array("TIME_RANGE", DataType::tp_double, 1, {2}, "Time range"),
            make_scalar("OBSERVER", DataType::tp_string, "Observer name"),
            make_array("LOG", DataType::tp_string, 1, {}, "Observation log"),
            make_scalar("SCHEDULE_TYPE", DataType::tp_string, "Schedule type"),
            make_array("SCHEDULE", DataType::tp_string, 1, {}, "Schedule"),
            make_scalar("PROJECT", DataType::tp_string, "Project ID"),
            make_scalar("RELEASE_DATE", DataType::tp_double, "Release date"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME_RANGE" || col.name == "RELEASE_DATE") {
                add_epoch_keywords(col);
            }
        }
        return cols;
    }
    if (name == "POINTING") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("ANTENNA_ID", DataType::tp_int, "Antenna ID"),
            make_scalar("TIME", DataType::tp_double, "Time"),
            make_scalar("INTERVAL", DataType::tp_double, "Interval"),
            make_scalar("NAME", DataType::tp_string, "Pointing name"),
            make_scalar("NUM_POLY", DataType::tp_int, "Polynomial order"),
            make_scalar("TIME_ORIGIN", DataType::tp_double, "Time origin"),
            make_array("DIRECTION", DataType::tp_double, 2, {}, "Pointing direction"),
            make_array("TARGET", DataType::tp_double, 2, {}, "Target direction"),
            make_scalar("TRACKING", DataType::tp_bool, "Tracking"),
        };
        for (auto& col : cols) {
            if (col.name == "TIME" || col.name == "TIME_ORIGIN") {
                add_epoch_keywords(col);
            } else if (col.name == "INTERVAL") {
                add_unit_keyword(col, "s");
            } else if (col.name == "DIRECTION" || col.name == "TARGET") {
                add_direction_keywords(col);
            }
        }
        return cols;
    }
    if (name == "POLARIZATION") {
        return {
            make_scalar("NUM_CORR", DataType::tp_int, "Number of correlations"),
            make_array("CORR_TYPE", DataType::tp_int, 1, {}, "Correlation type"),
            make_array("CORR_PRODUCT", DataType::tp_int, 2, {}, "Correlation product"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
    }
    if (name == "PROCESSOR") {
        return {
            make_scalar("TYPE", DataType::tp_string, "Processor type"),
            make_scalar("SUB_TYPE", DataType::tp_string, "Sub-type"),
            make_scalar("TYPE_ID", DataType::tp_int, "Type ID"),
            make_scalar("MODE_ID", DataType::tp_int, "Mode ID"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
    }
    if (name == "SPECTRAL_WINDOW") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("NUM_CHAN", DataType::tp_int, "Number of channels"),
            make_scalar("NAME", DataType::tp_string, "SPW name"),
            make_scalar("REF_FREQUENCY", DataType::tp_double, "Reference frequency"),
            make_array("CHAN_FREQ", DataType::tp_double, 1, {}, "Channel frequencies"),
            make_array("CHAN_WIDTH", DataType::tp_double, 1, {}, "Channel widths"),
            make_array("EFFECTIVE_BW", DataType::tp_double, 1, {}, "Effective bandwidths"),
            make_array("RESOLUTION", DataType::tp_double, 1, {}, "Spectral resolution"),
            make_scalar("MEAS_FREQ_REF", DataType::tp_int, "Frequency measure ref"),
            make_scalar("TOTAL_BANDWIDTH", DataType::tp_double, "Total bandwidth"),
            make_scalar("NET_SIDEBAND", DataType::tp_int, "Net sideband"),
            make_scalar("IF_CONV_CHAIN", DataType::tp_int, "IF conversion chain"),
            make_scalar("FREQ_GROUP", DataType::tp_int, "Frequency group"),
            make_scalar("FREQ_GROUP_NAME", DataType::tp_string, "Frequency group name"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
        for (auto& col : cols) {
            if (col.name == "CHAN_FREQ" || col.name == "REF_FREQUENCY") {
                add_frequency_keywords(col);
            } else if (col.name == "CHAN_WIDTH" || col.name == "EFFECTIVE_BW" ||
                       col.name == "RESOLUTION" || col.name == "TOTAL_BANDWIDTH") {
                add_unit_keyword(col, "Hz");
            }
        }
        return cols;
    }
    if (name == "STATE") {
        auto cols = std::vector<ColumnDesc>{
            make_scalar("SIG", DataType::tp_bool, "Signal"),
            make_scalar("REF", DataType::tp_bool, "Reference"),
            make_scalar("CAL", DataType::tp_double, "Calibration"),
            make_scalar("LOAD", DataType::tp_double, "Load"),
            make_scalar("SUB_SCAN", DataType::tp_int, "Sub-scan"),
            make_scalar("OBS_MODE", DataType::tp_string, "Observing mode"),
            make_scalar("FLAG_ROW", DataType::tp_bool, "Row flag"),
        };
        for (auto& col : cols) {
            if (col.name == "CAL" || col.name == "LOAD") {
                add_unit_keyword(col, "K");
            }
        }
        return cols;
    }

    // Optional subtables — return empty, they get created on demand.
    return {};
}

} // namespace

TableDatFull make_ms_main_table_dat(bool include_data) {
    TableDatFull full;
    full.table_version = 2;
    full.row_count = 0;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "";

    // Build column descriptors. All columns go into a single StandardStMan,
    // matching casacore's default MS creation behaviour.
    auto required_cols = ms_required_main_columns();

    for (const auto& info : required_cols) {
        ColumnDesc col = to_column_desc(info, "StandardStMan", "StandardStMan");
        full.table_desc.columns.push_back(std::move(col));
    }

    // Add optional DATA column if requested (also in SSM).
    if (include_data) {
        const auto& data_info = ms_main_column_info(MsMainColumn::data);
        ColumnDesc col = to_column_desc(data_info, "StandardStMan", "StandardStMan");
        full.table_desc.columns.push_back(std::move(col));
    }

    // Add measure metadata to TIME, TIME_CENTROID, UVW, EXPOSURE, INTERVAL.
    // Add CATEGORY keyword to FLAG_CATEGORY (required by casacore MSv2).
    for (auto& col : full.table_desc.columns) {
        if (col.name == "TIME" || col.name == "TIME_CENTROID") {
            add_time_measure_keywords(col);
        } else if (col.name == "EXPOSURE" || col.name == "INTERVAL") {
            add_time_quantum_keywords(col);
        } else if (col.name == "UVW") {
            add_uvw_measure_keywords(col);
        } else if (col.name == "FLAG_CATEGORY") {
            // casacore requires CATEGORY keyword: empty Vector<String>.
            RecordArray<std::string> cat;
            cat.shape = {0};
            col.keywords.set("CATEGORY", RecordValue(std::move(cat)));
        }
    }

    // Single storage manager: SSM at seq 0.
    StorageManagerSetup ssm_sm;
    ssm_sm.type_name = "StandardStMan";
    ssm_sm.sequence_number = 0;
    full.storage_managers.push_back(ssm_sm);

    // Column manager setups — all columns bound to SSM seq 0.
    for (const auto& col : full.table_desc.columns) {
        ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        if (!col.shape.empty()) {
            cms.has_shape = true;
            cms.shape = col.shape;
        }
        full.column_setups.push_back(std::move(cms));
    }

    full.post_td_row_count = 0;

    return full;
}

TableDatFull make_subtable_dat(const std::vector<ColumnDesc>& columns) {
    TableDatFull full;
    full.table_version = 2;
    full.row_count = 0;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "";
    full.table_desc.columns = columns;

    StorageManagerSetup sm;
    sm.type_name = "StandardStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : columns) {
        ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        if (!col.shape.empty()) {
            cms.has_shape = true;
            cms.shape = col.shape;
        }
        full.column_setups.push_back(std::move(cms));
    }

    full.post_td_row_count = 0;

    // Generate SSM blob.
    SsmWriter ssm;
    ssm.setup(columns, 0, false, "StandardStMan");
    full.storage_managers[0].data_blob = ssm.make_blob();

    return full;
}

void write_table_info(const std::filesystem::path& dir, std::string_view type_string,
                      std::string_view sub_type) {
    const auto info_path = dir / "table.info";
    std::ofstream out(info_path);
    if (!out) {
        throw std::runtime_error("cannot write: " + info_path.string());
    }
    out << "Type = " << type_string << "\n"
        << "SubType = " << sub_type << "\n";
}

std::string read_table_info_type(const std::filesystem::path& dir) {
    const auto info_path = dir / "table.info";
    std::ifstream in(info_path);
    if (!in) {
        return {};
    }
    std::string line;
    if (std::getline(in, line)) {
        const std::string prefix = "Type = ";
        if (line.substr(0, prefix.size()) == prefix) {
            return line.substr(prefix.size());
        }
    }
    return {};
}

MeasurementSet MeasurementSet::create(const std::filesystem::path& path, bool include_data) {
    if (std::filesystem::exists(path)) {
        throw std::runtime_error("MeasurementSet path already exists: " + path.string());
    }

    // Build main table.dat.
    auto table_dat = make_ms_main_table_dat(include_data);

    // Set MS_VERSION = 2.0 (required by casacore's MeasurementSet::checkVersion()).
    table_dat.table_desc.keywords.set("MS_VERSION", RecordValue(2.0F));

    // Add subtable keyword references to table-level keywords.
    for (const auto& st_name : ms_required_subtable_names()) {
        // Subtable keyword value is "Table: <relative_path>".
        table_dat.table_desc.keywords.set(
            st_name, RecordValue(std::string("Table: ././" + st_name)));
    }

    // Create the SSM writer for an empty table (0 rows) to get the blob.
    {
        std::vector<ColumnDesc> ssm_cols;
        for (const auto& col : table_dat.table_desc.columns) {
            if (col.dm_type == "StandardStMan") {
                ssm_cols.push_back(col);
            }
        }
        SsmWriter ssm;
        ssm.setup(ssm_cols, 0, false, "StandardStMan");
        table_dat.storage_managers[0].data_blob = ssm.make_blob();
    }

    // Create the TSM blob for table.dat (if TSM columns exist).
    std::vector<ColumnDesc> tsm_cols;
    for (const auto& col : table_dat.table_desc.columns) {
        if (col.dm_type != "StandardStMan") {
            tsm_cols.push_back(col);
        }
    }
    if (!tsm_cols.empty() && table_dat.storage_managers.size() > 1) {
        // Ensure all TSM columns have a shape for the empty writer.
        // Variable-shape columns get a temporary shape of [1].
        for (auto& col : tsm_cols) {
            if (col.shape.empty()) {
                col.shape = {1};
                col.ndim = 1;
                col.options = 4; // FixedShape
            }
        }
        TiledStManWriter tsm;
        tsm.setup("TiledColumnStMan", "TiledMS", tsm_cols, 0);
        table_dat.storage_managers[1].data_blob = tsm.make_blob();
    }

    // Create root directory.
    std::filesystem::create_directories(path);

    // Write table.dat.
    auto bytes = serialize_table_dat_full(table_dat);
    {
        std::ofstream out(path / "table.dat", std::ios::binary);
        if (!out) {
            throw std::runtime_error("cannot write: " + (path / "table.dat").string());
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    // Write empty .f0 for SSM.
    {
        std::vector<ColumnDesc> ssm_cols;
        for (const auto& col : table_dat.table_desc.columns) {
            if (col.dm_type == "StandardStMan") {
                ssm_cols.push_back(col);
            }
        }
        SsmWriter ssm;
        ssm.setup(ssm_cols, 0, false, "StandardStMan");
        ssm.write_file(path.string(), 0);
    }

    // Write empty TSM files (.f1 header + .f1_TSM0 data) so casacore can
    // open all declared storage managers without "file not found" errors.
    if (!tsm_cols.empty() && table_dat.storage_managers.size() > 1) {
        TiledStManWriter tsm;
        tsm.setup("TiledColumnStMan", "TiledMS", tsm_cols, 0);
        tsm.write_files(path.string(), 1);
    }

    // Write empty .f0i for SSM indirect (variable-shape) array columns.
    {
        std::vector<ColumnDesc> ssm_cols;
        for (const auto& col : table_dat.table_desc.columns) {
            if (col.dm_type == "StandardStMan") {
                ssm_cols.push_back(col);
            }
        }
        if (has_indirect_array_columns(ssm_cols)) {
            write_empty_array_file(path, 0);
        }
    }

    // Write table.info.
    write_table_info(path, "Measurement Set", "");

    // Write table.lock (empty).
    { std::ofstream(path / "table.lock"); }

    // Create required subtable directories.
    for (const auto& st_name : ms_required_subtable_names()) {
        auto st_path = path / st_name;
        auto st_cols = make_required_subtable_columns(st_name);
        create_empty_subtable(st_path, st_cols);
    }

    // Return an opened MS.
    return MeasurementSet::open(path);
}

MeasurementSet MeasurementSet::open(const std::filesystem::path& path) {
    if (!std::filesystem::is_directory(path)) {
        throw std::runtime_error("MeasurementSet path is not a directory: " + path.string());
    }

    const auto table_dat_path = path / "table.dat";
    if (!std::filesystem::exists(table_dat_path)) {
        throw std::runtime_error("missing table.dat in MeasurementSet: " + path.string());
    }

    MeasurementSet ms;
    ms.root_path_ = path;
    ms.main_table_ = Table::open(path);

    return ms;
}

const Table& MeasurementSet::subtable(std::string_view name) const {
    const std::string key(name);
    auto it = subtables_.find(key);
    if (it != subtables_.end()) {
        return it->second;
    }

    const auto st_path = root_path_ / key;
    if (!std::filesystem::is_directory(st_path)) {
        throw std::runtime_error("subtable not found: " + key);
    }

    auto [inserted, _] = subtables_.emplace(key, Table::open(st_path));
    return inserted->second;
}

Table& MeasurementSet::subtable(std::string_view name) {
    const std::string key(name);
    auto it = subtables_.find(key);
    if (it != subtables_.end()) {
        return it->second;
    }

    const auto st_path = root_path_ / key;
    if (!std::filesystem::is_directory(st_path)) {
        throw std::runtime_error("subtable not found: " + key);
    }

    auto [inserted, _] = subtables_.emplace(key, Table::open(st_path));
    return inserted->second;
}

std::vector<std::string> MeasurementSet::subtable_names() const {
    std::vector<std::string> result;
    for (const auto& name : ms_all_subtable_names()) {
        if (std::filesystem::is_directory(root_path_ / name)) {
            result.push_back(name);
        }
    }
    return result;
}

bool MeasurementSet::has_subtable(std::string_view name) const {
    return std::filesystem::is_directory(root_path_ / std::string(name));
}

void MeasurementSet::flush() {
    auto bytes = serialize_table_dat_full(main_table_.table_dat());
    std::ofstream out(root_path_ / "table.dat", std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot flush table.dat");
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

} // namespace casacore_mini
