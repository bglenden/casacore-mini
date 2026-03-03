// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/record_metadata.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {
namespace {

[[nodiscard]] const Record* as_sub_record(const RecordValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto* ptr = std::get_if<RecordValue::record_ptr>(&value->storage());
    if (ptr == nullptr || !*ptr) {
        return nullptr;
    }
    return ptr->get();
}

[[nodiscard]] std::optional<std::string> as_string(const RecordValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* s = std::get_if<std::string>(&value->storage());
    if (s == nullptr) {
        return std::nullopt;
    }
    return *s;
}

[[nodiscard]] std::optional<double> as_double(const RecordValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* d = std::get_if<double>(&value->storage())) {
        return *d;
    }
    if (const auto* f = std::get_if<float>(&value->storage())) {
        return static_cast<double>(*f);
    }
    if (const auto* i32 = std::get_if<std::int32_t>(&value->storage())) {
        return static_cast<double>(*i32);
    }
    if (const auto* i64 = std::get_if<std::int64_t>(&value->storage())) {
        return static_cast<double>(*i64);
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::string> as_string_array(const RecordValue* value) {
    if (value == nullptr) {
        return {};
    }
    const auto* arr = std::get_if<RecordValue::string_array>(&value->storage());
    if (arr == nullptr) {
        return {};
    }
    return arr->elements;
}

[[nodiscard]] std::vector<double> as_double_array(const RecordValue* value) {
    if (value == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<RecordValue::double_array>(&value->storage())) {
        std::vector<double> result(arr->elements.begin(), arr->elements.end());
        return result;
    }
    if (const auto* arr = std::get_if<RecordValue::float_array>(&value->storage())) {
        std::vector<double> result;
        result.reserve(arr->elements.size());
        for (const auto elem : arr->elements) {
            result.push_back(static_cast<double>(elem));
        }
        return result;
    }
    return {};
}

[[nodiscard]] std::vector<std::int64_t> as_int64_array(const RecordValue* value) {
    if (value == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<RecordValue::int64_array>(&value->storage())) {
        return arr->elements;
    }
    if (const auto* arr = std::get_if<RecordValue::int32_array>(&value->storage())) {
        std::vector<std::int64_t> result;
        result.reserve(arr->elements.size());
        for (const auto elem : arr->elements) {
            result.push_back(static_cast<std::int64_t>(elem));
        }
        return result;
    }
    return {};
}

[[nodiscard]] std::optional<std::string> find_string(const Record& record,
                                                     const std::string_view key) {
    return as_string(record.find(key));
}

[[nodiscard]] std::optional<double> find_double(const Record& record, const std::string_view key) {
    return as_double(record.find(key));
}

[[nodiscard]] std::vector<std::string> find_string_array(const Record& record,
                                                         const std::string_view key) {
    return as_string_array(record.find(key));
}

[[nodiscard]] std::vector<std::int64_t> find_int64_array(const Record& record,
                                                         const std::string_view key) {
    return as_int64_array(record.find(key));
}

[[nodiscard]] std::vector<double> find_double_array(const Record& record,
                                                    const std::string_view key) {
    return as_double_array(record.find(key));
}

[[nodiscard]] bool has_measure_data(const MeasureColumnMetadata& metadata) {
    return !metadata.quantum_units.empty() || metadata.measure_type.has_value() ||
           metadata.measure_reference.has_value();
}

[[nodiscard]] MeasureColumnMetadata extract_column_measures(std::string column_name,
                                                            const Record& record) {
    MeasureColumnMetadata metadata{
        .column_name = std::move(column_name),
        .quantum_units = {},
        .measure_type = std::nullopt,
        .measure_reference = std::nullopt,
    };

    metadata.quantum_units = find_string_array(record, "QuantumUnits");
    if (metadata.quantum_units.empty()) {
        if (const auto unit = find_string(record, "UNIT")) {
            metadata.quantum_units = {*unit};
        }
    }

    metadata.measure_type = find_string(record, "MEASURE_TYPE");
    metadata.measure_reference = find_string(record, "MEASURE_REFERENCE");

    const auto* measinfo = as_sub_record(record.find("MEASINFO"));
    if (measinfo != nullptr) {
        if (!metadata.measure_type.has_value()) {
            metadata.measure_type = find_string(*measinfo, "type");
        }
        if (!metadata.measure_reference.has_value()) {
            metadata.measure_reference = find_string(*measinfo, "Ref");
            if (!metadata.measure_reference.has_value()) {
                metadata.measure_reference = find_string(*measinfo, "ref");
            }
        }
    }

    return metadata;
}

void populate_coordinate_metadata(const Record& table_keywords,
                                  CoordinateKeywordMetadata* metadata) {
    const auto* coords = as_sub_record(table_keywords.find("coords"));
    if (coords == nullptr) {
        return;
    }

    metadata->has_coords = true;
    if (const auto* obsdate = as_sub_record(coords->find("obsdate"))) {
        metadata->obsdate_type = find_string(*obsdate, "type");
        metadata->obsdate_reference = find_string(*obsdate, "refer");
    }

    if (const auto* direction0 = as_sub_record(coords->find("direction0"))) {
        metadata->direction_system = find_string(*direction0, "system");
        metadata->direction_projection = find_string(*direction0, "projection");
        metadata->direction_projection_parameters =
            find_double_array(*direction0, "projection_parameters");
        metadata->direction_crval = find_double_array(*direction0, "crval");
        metadata->direction_crpix = find_double_array(*direction0, "crpix");
        metadata->direction_cdelt = find_double_array(*direction0, "cdelt");
        metadata->direction_pc = find_double_array(*direction0, "pc");
        metadata->direction_axes = find_string_array(*direction0, "axes");
        metadata->direction_units = find_string_array(*direction0, "units");
        metadata->direction_conversion_system = find_string(*direction0, "conversionSystem");
        metadata->direction_longpole = find_double(*direction0, "longpole");
        metadata->direction_latpole = find_double(*direction0, "latpole");
    }

    metadata->worldmap0 = find_int64_array(*coords, "worldmap0");
    metadata->worldreplace0 = find_double_array(*coords, "worldreplace0");
    metadata->pixelmap0 = find_int64_array(*coords, "pixelmap0");
    metadata->pixelreplace0 = find_double_array(*coords, "pixelreplace0");
}

} // namespace

MeasureCoordinateMetadata
extract_record_metadata(const Record& table_keywords,
                        const std::vector<std::pair<std::string, Record>>& column_keywords) {
    MeasureCoordinateMetadata metadata;

    metadata.measure_columns.reserve(column_keywords.size());
    for (const auto& [column_name, column_record] : column_keywords) {
        auto measure_column = extract_column_measures(column_name, column_record);
        if (has_measure_data(measure_column)) {
            metadata.measure_columns.push_back(std::move(measure_column));
        }
    }

    populate_coordinate_metadata(table_keywords, &metadata.coordinates);
    return metadata;
}

} // namespace casacore_mini
