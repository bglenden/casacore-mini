#include "casacore_mini/ms_enums.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {

namespace {

// Static column info table for main-table columns.
// Order must match MsMainColumn enum values.
// NOLINTBEGIN(bugprone-branch-clone)
const std::array<MsColumnInfo, 29>& main_column_table() {
    static const std::array<MsColumnInfo, 29> table = {{
        // Required columns (21):
        {"ANTENNA1", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of first antenna in baseline", true},
        {"ANTENNA2", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of second antenna in baseline", true},
        {"ARRAY_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of subarray", true},
        {"DATA_DESC_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of data description", true},
        {"EXPOSURE", DataType::tp_double, ColumnKind::scalar, 0, {}, "Effective integration time", true},
        {"FEED1", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of feed for ANTENNA1", true},
        {"FEED2", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of feed for ANTENNA2", true},
        {"FIELD_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of field", true},
        {"FLAG", DataType::tp_bool, ColumnKind::array, 2, {}, "Flags per channel per correlation", true},
        {"FLAG_CATEGORY", DataType::tp_bool, ColumnKind::array, 3, {}, "Flag categories", true},
        {"FLAG_ROW", DataType::tp_bool, ColumnKind::scalar, 0, {}, "Row flag", true},
        {"INTERVAL", DataType::tp_double, ColumnKind::scalar, 0, {}, "Sampling interval", true},
        {"OBSERVATION_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of observation", true},
        {"PROCESSOR_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of processor", true},
        {"SCAN_NUMBER", DataType::tp_int, ColumnKind::scalar, 0, {}, "Scan number", true},
        {"SIGMA", DataType::tp_float, ColumnKind::array, 1, {}, "Estimated noise per correlation", true},
        {"STATE_ID", DataType::tp_int, ColumnKind::scalar, 0, {}, "ID of state", true},
        {"TIME", DataType::tp_double, ColumnKind::scalar, 0, {}, "Modified Julian Day", true},
        {"TIME_CENTROID", DataType::tp_double, ColumnKind::scalar, 0, {}, "Time centroid MJD", true},
        {"UVW", DataType::tp_double, ColumnKind::array, 1, {3}, "UVW coordinates", true},
        {"WEIGHT", DataType::tp_float, ColumnKind::array, 1, {}, "Weight per correlation", true},
        // Optional columns:
        {"DATA", DataType::tp_complex, ColumnKind::array, 2, {}, "Complex visibility data", false},
        {"FLOAT_DATA", DataType::tp_float, ColumnKind::array, 2, {}, "Float visibility data", false},
        {"CORRECTED_DATA", DataType::tp_complex, ColumnKind::array, 2, {}, "Corrected visibility data", false},
        {"MODEL_DATA", DataType::tp_complex, ColumnKind::array, 2, {}, "Model visibility data", false},
        {"IMAGING_WEIGHT", DataType::tp_float, ColumnKind::array, 1, {}, "Imaging weight per channel", false},
        {"WEIGHT_SPECTRUM", DataType::tp_float, ColumnKind::array, 2, {}, "Weight per channel per correlation", false},
        {"SIGMA_SPECTRUM", DataType::tp_float, ColumnKind::array, 2, {}, "Sigma per channel per correlation", false},
        {"FLAG_SPECTRUM", DataType::tp_bool, ColumnKind::array, 2, {}, "Spectral flags", false},
    }};
    return table;
}
// NOLINTEND(bugprone-branch-clone)

struct KeywordInfo {
    std::string_view name;
    bool required;
};

const std::array<KeywordInfo, 17>& keyword_table() {
    static const std::array<KeywordInfo, 17> table = {{
        {"ANTENNA", true},
        {"DATA_DESCRIPTION", true},
        {"DOPPLER", false},
        {"FEED", true},
        {"FIELD", true},
        {"FLAG_CMD", true},
        {"FREQ_OFFSET", false},
        {"HISTORY", true},
        {"OBSERVATION", true},
        {"POINTING", true},
        {"POLARIZATION", true},
        {"PROCESSOR", true},
        {"SOURCE", false},
        {"SPECTRAL_WINDOW", true},
        {"STATE", true},
        {"SYSCAL", false},
        {"WEATHER", false},
    }};
    return table;
}

} // namespace

const MsColumnInfo& ms_main_column_info(MsMainColumn col) {
    const auto idx = static_cast<std::size_t>(static_cast<std::int32_t>(col));
    const auto& table = main_column_table();
    if (idx >= table.size()) {
        throw std::out_of_range("invalid MsMainColumn enum value");
    }
    return table[idx];
}

std::optional<MsMainColumn> ms_main_column_from_name(std::string_view name) {
    const auto& table = main_column_table();
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].name == name) {
            return static_cast<MsMainColumn>(static_cast<std::int32_t>(i));
        }
    }
    return std::nullopt;
}

std::string_view ms_main_keyword_name(MsMainKeyword kw) {
    const auto idx = static_cast<std::size_t>(static_cast<std::int32_t>(kw));
    const auto& table = keyword_table();
    if (idx >= table.size()) {
        throw std::out_of_range("invalid MsMainKeyword enum value");
    }
    return table[idx].name;
}

bool ms_main_keyword_required(MsMainKeyword kw) {
    const auto idx = static_cast<std::size_t>(static_cast<std::int32_t>(kw));
    const auto& table = keyword_table();
    if (idx >= table.size()) {
        throw std::out_of_range("invalid MsMainKeyword enum value");
    }
    return table[idx].required;
}

std::vector<MsColumnInfo> ms_required_main_columns() {
    const auto& table = main_column_table();
    std::vector<MsColumnInfo> result;
    result.reserve(table.size());
    for (const auto& col : table) {
        if (col.required) {
            result.push_back(col);
        }
    }
    return result;
}

std::vector<std::string> ms_required_subtable_names() {
    const auto& table = keyword_table();
    std::vector<std::string> result;
    result.reserve(table.size());
    for (const auto& kw : table) {
        if (kw.required) {
            result.emplace_back(kw.name);
        }
    }
    return result;
}

std::vector<std::string> ms_all_subtable_names() {
    const auto& table = keyword_table();
    std::vector<std::string> result;
    result.reserve(table.size());
    for (const auto& kw : table) {
        result.emplace_back(kw.name);
    }
    return result;
}

} // namespace casacore_mini
