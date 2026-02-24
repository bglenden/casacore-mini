#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/linear_coordinate.hpp"
#include "casacore_mini/quality_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"
#include "casacore_mini/tabular_coordinate.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

struct CoordTypeEntry {
    CoordinateType type;
    const char* name;
};

constexpr CoordTypeEntry kCoordTypes[] = {
    {CoordinateType::linear, "linear"},     {CoordinateType::direction, "direction"},
    {CoordinateType::spectral, "spectral"}, {CoordinateType::stokes, "stokes"},
    {CoordinateType::tabular, "tabular"},   {CoordinateType::quality, "quality"},
};

const std::string* find_string(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    return std::get_if<std::string>(&val->storage());
}

} // namespace

std::string coordinate_type_to_string(CoordinateType t) {
    for (const auto& e : kCoordTypes) {
        if (e.type == t) {
            return e.name;
        }
    }
    return "unknown";
}

CoordinateType string_to_coordinate_type(std::string_view s) {
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& e : kCoordTypes) {
        if (lower == e.name) {
            return e.type;
        }
    }
    throw std::invalid_argument("Unknown coordinate type: " + std::string(s));
}

std::unique_ptr<Coordinate> Coordinate::restore(const Record& rec) {
    const auto* type_str = find_string(rec, "coordinate_type");
    if (type_str == nullptr) {
        throw std::invalid_argument("Coordinate::restore: missing 'coordinate_type' field");
    }

    CoordinateType ct = string_to_coordinate_type(*type_str);
    switch (ct) {
    case CoordinateType::direction:
        return DirectionCoordinate::from_record(rec);
    case CoordinateType::spectral:
        return SpectralCoordinate::from_record(rec);
    case CoordinateType::stokes:
        return StokesCoordinate::from_record(rec);
    case CoordinateType::linear:
        return LinearCoordinate::from_record(rec);
    case CoordinateType::tabular:
        return TabularCoordinate::from_record(rec);
    case CoordinateType::quality:
        return QualityCoordinate::from_record(rec);
    }

    throw std::invalid_argument("Coordinate::restore: unhandled type " + *type_str);
}

} // namespace casacore_mini
