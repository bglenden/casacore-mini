#include "casacore_mini/quantity.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace casacore_mini {

namespace {

// Conversion factors to a canonical base unit per dimension.
// Angle -> radians, Time -> seconds, Length -> meters, Frequency -> Hz.
struct UnitInfo {
    std::string_view dimension;
    double to_base; // multiply value by this to get base units
};

// NOLINTBEGIN(cert-err58-cpp)
const std::unordered_map<std::string_view, UnitInfo> kUnitTable = {
    // Angle
    {"rad", {"angle", 1.0}},
    {"deg", {"angle", M_PI / 180.0}},
    {"arcmin", {"angle", M_PI / 10800.0}},
    {"arcsec", {"angle", M_PI / 648000.0}},
    {"\"", {"angle", M_PI / 648000.0}},
    {"'", {"angle", M_PI / 10800.0}},
    // Time
    {"s", {"time", 1.0}},
    {"min", {"time", 60.0}},
    {"h", {"time", 3600.0}},
    {"d", {"time", 86400.0}},
    // Length
    {"m", {"length", 1.0}},
    {"km", {"length", 1000.0}},
    // Frequency
    {"Hz", {"frequency", 1.0}},
    {"kHz", {"frequency", 1.0e3}},
    {"MHz", {"frequency", 1.0e6}},
    {"GHz", {"frequency", 1.0e9}},
    // Velocity
    {"m/s", {"velocity", 1.0}},
    {"km/s", {"velocity", 1000.0}},
    // Dimensionless
    {"", {"dimensionless", 1.0}},
};
// NOLINTEND(cert-err58-cpp)

} // namespace

Quantity convert_quantity(const Quantity& q, std::string_view target_unit) {
    if (q.unit == target_unit) {
        return q;
    }

    auto src_it = kUnitTable.find(q.unit);
    auto dst_it = kUnitTable.find(target_unit);

    if (src_it == kUnitTable.end()) {
        throw std::invalid_argument("Unknown source unit: " + q.unit);
    }
    if (dst_it == kUnitTable.end()) {
        throw std::invalid_argument("Unknown target unit: " + std::string(target_unit));
    }

    if (src_it->second.dimension != dst_it->second.dimension) {
        throw std::invalid_argument("Incompatible units: " + q.unit + " -> " +
                                    std::string(target_unit));
    }

    double base_value = q.value * src_it->second.to_base;
    return Quantity{base_value / dst_it->second.to_base, std::string(target_unit)};
}

} // namespace casacore_mini
