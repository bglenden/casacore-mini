#include "casacore_mini/measure_coord_metadata.hpp"

#include "casacore_mini/keyword_record.hpp"

#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace casacore_mini {
namespace {

[[nodiscard]] const KeywordRecord* as_record(const KeywordValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto* storage = std::get_if<KeywordValue::record_ptr>(&value->storage());
    if (storage == nullptr || !*storage) {
        return nullptr;
    }
    return storage->get();
}

[[nodiscard]] const KeywordArray* as_array(const KeywordValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto* storage = std::get_if<KeywordValue::array_ptr>(&value->storage());
    if (storage == nullptr || !*storage) {
        return nullptr;
    }
    return storage->get();
}

[[nodiscard]] std::optional<std::string> value_as_string(const KeywordValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* string_value = std::get_if<std::string>(&value->storage());
    if (string_value == nullptr) {
        return std::nullopt;
    }
    return *string_value;
}

[[nodiscard]] std::optional<std::int64_t> value_as_int64(const KeywordValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const auto* int_value = std::get_if<std::int64_t>(&value->storage());
    if (int_value == nullptr) {
        return std::nullopt;
    }
    return *int_value;
}

[[nodiscard]] std::optional<double> value_as_double(const KeywordValue* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    if (const auto* double_value = std::get_if<double>(&value->storage())) {
        return *double_value;
    }
    if (const auto* int_value = std::get_if<std::int64_t>(&value->storage())) {
        return static_cast<double>(*int_value);
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<std::string> value_as_string_array(const KeywordValue* value) {
    std::vector<std::string> result;
    const auto* array = as_array(value);
    if (array == nullptr) {
        return result;
    }
    result.reserve(array->elements.size());
    for (const auto& element : array->elements) {
        const auto parsed = value_as_string(&element);
        if (!parsed.has_value()) {
            return {};
        }
        result.push_back(*parsed);
    }
    return result;
}

[[nodiscard]] std::vector<std::int64_t> value_as_int64_array(const KeywordValue* value) {
    std::vector<std::int64_t> result;
    const auto* array = as_array(value);
    if (array == nullptr) {
        return result;
    }
    result.reserve(array->elements.size());
    for (const auto& element : array->elements) {
        const auto parsed = value_as_int64(&element);
        if (!parsed.has_value()) {
            return {};
        }
        result.push_back(*parsed);
    }
    return result;
}

[[nodiscard]] std::vector<double> value_as_double_array(const KeywordValue* value) {
    std::vector<double> result;
    const auto* array = as_array(value);
    if (array == nullptr) {
        return result;
    }
    result.reserve(array->elements.size());
    for (const auto& element : array->elements) {
        const auto parsed = value_as_double(&element);
        if (!parsed.has_value()) {
            return {};
        }
        result.push_back(*parsed);
    }
    return result;
}

[[nodiscard]] std::optional<std::string> find_string(const KeywordRecord& record,
                                                     const std::string_view key) {
    return value_as_string(record.find(key));
}

[[nodiscard]] std::optional<double> find_double(const KeywordRecord& record,
                                                const std::string_view key) {
    return value_as_double(record.find(key));
}

[[nodiscard]] std::vector<std::string> find_string_array(const KeywordRecord& record,
                                                         const std::string_view key) {
    return value_as_string_array(record.find(key));
}

[[nodiscard]] std::vector<std::int64_t> find_int64_array(const KeywordRecord& record,
                                                         const std::string_view key) {
    return value_as_int64_array(record.find(key));
}

[[nodiscard]] std::vector<double> find_double_array(const KeywordRecord& record,
                                                    const std::string_view key) {
    return value_as_double_array(record.find(key));
}

[[nodiscard]] bool has_measure_data(const MeasureColumnMetadata& metadata) {
    return !metadata.quantum_units.empty() || metadata.measure_type.has_value() ||
           metadata.measure_reference.has_value();
}

[[nodiscard]] MeasureColumnMetadata parse_measure_column_metadata(std::string column_name,
                                                                  const KeywordRecord& record) {
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

    const auto* measinfo = as_record(record.find("MEASINFO"));
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

void populate_coordinate_metadata(const KeywordRecord& table_keywords,
                                  CoordinateKeywordMetadata* metadata) {
    const auto* coords = as_record(table_keywords.find("coords"));
    if (coords == nullptr) {
        return;
    }

    metadata->has_coords = true;
    if (const auto* obsdate = as_record(coords->find("obsdate"))) {
        metadata->obsdate_type = find_string(*obsdate, "type");
        metadata->obsdate_reference = find_string(*obsdate, "refer");
    }

    if (const auto* direction0 = as_record(coords->find("direction0"))) {
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
parse_showtableinfo_measure_coordinate_metadata(const std::string_view showtableinfo_text) {
    MeasureCoordinateMetadata metadata;
    ShowtableinfoKeywords parsed;
    try {
        parsed = parse_showtableinfo_keywords(showtableinfo_text);
    } catch (const std::exception&) {
        return metadata;
    }

    metadata.measure_columns.reserve(parsed.column_keywords.size());
    for (const auto& [column_name, column_keywords] : parsed.column_keywords) {
        auto measure_column = parse_measure_column_metadata(column_name, column_keywords);
        if (has_measure_data(measure_column)) {
            metadata.measure_columns.push_back(std::move(measure_column));
        }
    }

    populate_coordinate_metadata(parsed.table_keywords, &metadata.coordinates);
    return metadata;
}

} // namespace casacore_mini
