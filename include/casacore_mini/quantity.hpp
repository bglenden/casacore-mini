#pragma once

#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Thin quantity wrapper: a numeric value paired with a unit string.

/// A physical quantity: a double value with a unit string.
///
/// Supports conversion between known unit pairs (rad/deg, s/d, m/km, Hz/GHz/MHz).
/// Unknown or incompatible unit conversions throw `std::invalid_argument`.
struct Quantity {
    double value = 0.0;
    std::string unit;

    /// Default-construct a dimensionless zero.
    Quantity() = default;
    /// Construct with value and unit.
    Quantity(double v, std::string u) : value(v), unit(std::move(u)) {}

    [[nodiscard]] bool operator==(const Quantity& other) const = default;
};

/// Convert a quantity to a different unit.
///
/// Supported conversions:
/// - Angle: rad <-> deg
/// - Time: s <-> d (days), s <-> h (hours), s <-> min
/// - Length: m <-> km
/// - Frequency: Hz <-> kHz <-> MHz <-> GHz
///
/// @throws std::invalid_argument if the conversion is not supported.
[[nodiscard]] Quantity convert_quantity(const Quantity& q, std::string_view target_unit);

} // namespace casacore_mini
