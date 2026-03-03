// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_measure_desc.hpp"
#include "casacore_mini/measure_record.hpp"

#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

// Helper: find an optional string field in a record.
const std::string* find_string(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    return std::get_if<std::string>(&val->storage());
}

// Helper: find an optional sub-record field.
const Record* find_sub_record(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    const auto* rp = std::get_if<RecordValue::record_ptr>(&val->storage());
    if (rp == nullptr || *rp == nullptr) {
        return nullptr;
    }
    return rp->get();
}

// Helper: find an optional bool field.
const bool* find_bool(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    return std::get_if<bool>(&val->storage());
}

// Helper: get string array from a Record field.
// Supports both string_array and inspecting a record list.
std::vector<std::string> get_string_array(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return {};
    }
    // Try string_array first.
    if (const auto* arr = std::get_if<RecordValue::string_array>(&val->storage())) {
        return arr->elements;
    }
    return {};
}

// Helper: get uint32 array from a Record field (stored as int32_array or uint32_array).
std::vector<std::uint32_t> get_uint_array(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<RecordValue::uint32_array>(&val->storage())) {
        return arr->elements;
    }
    // Casacore may store these as signed int32 arrays.
    if (const auto* arr = std::get_if<RecordValue::int32_array>(&val->storage())) {
        std::vector<std::uint32_t> result;
        result.reserve(arr->elements.size());
        for (auto v : arr->elements) {
            result.push_back(static_cast<std::uint32_t>(v));
        }
        return result;
    }
    return {};
}

// Map a measure type string from MEASINFO to our MeasureType enum.
// Upstream uses "epoch", "direction", "position", "frequency", "doppler",
// "radialvelocity", "baseline", "uvw", "earthmagnetic".
MeasureType parse_measinfo_type(const std::string& type_str) {
    return string_to_measure_type(type_str);
}

// Parse a reference string for a given measure type.
MeasureRefType parse_ref_for_type(MeasureType type, const std::string& refer) {
    switch (type) {
    case MeasureType::epoch:
        return string_to_epoch_ref(refer);
    case MeasureType::direction:
        return string_to_direction_ref(refer);
    case MeasureType::position:
        return string_to_position_ref(refer);
    case MeasureType::frequency:
        return string_to_frequency_ref(refer);
    case MeasureType::doppler:
        return string_to_doppler_ref(refer);
    case MeasureType::radial_velocity:
        return string_to_radial_velocity_ref(refer);
    case MeasureType::baseline:
        return string_to_baseline_ref(refer);
    case MeasureType::uvw:
        return string_to_uvw_ref(refer);
    case MeasureType::earth_magnetic:
        return string_to_earth_magnetic_ref(refer);
    }
    throw std::invalid_argument("Unknown measure type in parse_ref_for_type");
}

} // namespace

std::optional<TableMeasDesc> read_table_measure_desc(const std::string& column_name,
                                                     const Record& column_keywords) {
    const auto* measinfo_rec = find_sub_record(column_keywords, "MEASINFO");
    if (measinfo_rec == nullptr) {
        return std::nullopt;
    }

    TableMeasDesc desc;
    desc.column_name = column_name;

    // Extract measure type.
    const auto* type_str = find_string(*measinfo_rec, "type");
    if (type_str == nullptr) {
        throw std::invalid_argument("MEASINFO missing 'type' field for column '" + column_name +
                                    "'");
    }
    desc.measure_type = parse_measinfo_type(*type_str);

    // Extract reference: fixed "Ref" or variable "VarRefCol".
    const auto* ref_str = find_string(*measinfo_rec, "Ref");
    const auto* var_ref = find_string(*measinfo_rec, "VarRefCol");
    if (var_ref != nullptr) {
        desc.var_ref_column = *var_ref;
        desc.tab_ref_types = get_string_array(*measinfo_rec, "TabRefTypes");
        desc.tab_ref_codes = get_uint_array(*measinfo_rec, "TabRefCodes");
        // Default ref from the first TabRefType if available.
        if (!desc.tab_ref_types.empty()) {
            desc.default_ref = parse_ref_for_type(desc.measure_type, desc.tab_ref_types[0]);
        } else {
            desc.default_ref = default_ref_for_type(desc.measure_type);
        }
    } else if (ref_str != nullptr) {
        desc.default_ref = parse_ref_for_type(desc.measure_type, *ref_str);
    } else {
        desc.default_ref = default_ref_for_type(desc.measure_type);
    }

    // Extract units from column keywords (not MEASINFO).
    desc.units = get_string_array(column_keywords, "QuantumUnits");

    // Extract variable units column if present.
    // (VariableUnits is stored at the column keyword level, not inside MEASINFO.)
    // We don't populate it here — that's handled by TableQuantumDesc.

    // Extract offset: fixed "RefOffMsr" or variable "RefOffCol".
    const auto* offset_rec = find_sub_record(*measinfo_rec, "RefOffMsr");
    if (offset_rec != nullptr) {
        desc.offset = measure_from_record(*offset_rec);
    }
    const auto* offset_col = find_string(*measinfo_rec, "RefOffCol");
    if (offset_col != nullptr) {
        desc.offset_column = *offset_col;
    }
    const auto* per_arr = find_bool(*measinfo_rec, "RefOffvarPerArr");
    if (per_arr != nullptr) {
        desc.offset_per_array = *per_arr;
    }

    return desc;
}

void write_table_measure_desc(const TableMeasDesc& desc, Record& column_keywords) {
    Record measinfo;

    // Type.
    measinfo.set("type", RecordValue(std::string(measure_type_to_string(desc.measure_type))));

    // Reference.
    if (desc.has_variable_ref()) {
        measinfo.set("VarRefCol", RecordValue(desc.var_ref_column));
        if (!desc.tab_ref_types.empty()) {
            measinfo.set("TabRefTypes", RecordValue(RecordValue::string_array{
                                            {static_cast<std::uint64_t>(desc.tab_ref_types.size())},
                                            desc.tab_ref_types}));
        }
        if (!desc.tab_ref_codes.empty()) {
            // Store as int32 array for casacore compatibility.
            std::vector<std::int32_t> codes;
            codes.reserve(desc.tab_ref_codes.size());
            for (auto c : desc.tab_ref_codes) {
                codes.push_back(static_cast<std::int32_t>(c));
            }
            measinfo.set("TabRefCodes",
                         RecordValue(RecordValue::int32_array{
                             {static_cast<std::uint64_t>(codes.size())}, std::move(codes)}));
        }
    } else {
        measinfo.set("Ref", RecordValue(measure_ref_to_string(desc.default_ref)));
    }

    // Offset.
    if (desc.offset.has_value()) {
        measinfo.set("RefOffMsr", RecordValue::from_record(measure_to_record(*desc.offset)));
    }
    if (!desc.offset_column.empty()) {
        measinfo.set("RefOffCol", RecordValue(desc.offset_column));
        measinfo.set("RefOffvarPerArr", RecordValue(desc.offset_per_array));
    }

    column_keywords.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    // QuantumUnits at the column keyword level.
    if (!desc.units.empty()) {
        column_keywords.set("QuantumUnits",
                            RecordValue(RecordValue::string_array{
                                {static_cast<std::uint64_t>(desc.units.size())}, desc.units}));
    }
}

std::optional<TableQuantumDesc> read_table_quantum_desc(const std::string& column_name,
                                                        const Record& column_keywords) {
    auto units = get_string_array(column_keywords, "QuantumUnits");
    const auto* var_units = find_string(column_keywords, "VariableUnits");

    if (units.empty() && var_units == nullptr) {
        return std::nullopt;
    }

    TableQuantumDesc desc;
    desc.column_name = column_name;
    desc.units = std::move(units);
    if (var_units != nullptr) {
        desc.var_units_column = *var_units;
    }
    return desc;
}

void write_table_quantum_desc(const TableQuantumDesc& desc, Record& column_keywords) {
    if (desc.has_variable_units()) {
        column_keywords.set("VariableUnits", RecordValue(desc.var_units_column));
    } else if (!desc.units.empty()) {
        column_keywords.set("QuantumUnits",
                            RecordValue(RecordValue::string_array{
                                {static_cast<std::uint64_t>(desc.units.size())}, desc.units}));
    }
}

} // namespace casacore_mini
