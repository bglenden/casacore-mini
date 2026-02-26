#pragma once

#include "casacore_mini/unit.hpp"

#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Physical quantity: numeric value + unit with arithmetic and conversion.

/// A physical quantity: a double value paired with a Unit.
///
/// Supports conversion between any dimensionally compatible units, arithmetic
/// between quantities, and scalar multiplication/division.  Backward-compatible
/// with the original `Quantity{double, string}` aggregate -- `q.value` is a
/// public double member.
class Quantity {
  public:
    /// The numeric value in the stored unit.  Public for backward compatibility.
    double value = 0.0;

    /// Default-construct a dimensionless zero.
    Quantity();
    /// Construct with value and unit string.
    Quantity(double v, std::string u);
    /// Construct with value and Unit object.
    Quantity(double v, const Unit& u);

    /// The unit string (e.g. "km/s").
    [[nodiscard]] const std::string& get_unit() const {
        return unit_.name();
    }

    /// The parsed Unit object.
    [[nodiscard]] const Unit& unit() const {
        return unit_;
    }

    /// The Unit's UnitVal (factor + dimensions).
    [[nodiscard]] const UnitVal& unit_value() const {
        return unit_.value();
    }

    // -- Conversion --

    /// Convert to a different unit (must be dimensionally conformant).
    /// @throws std::invalid_argument on dimension mismatch or unknown unit.
    [[nodiscard]] Quantity convert(const Unit& target) const;
    [[nodiscard]] Quantity convert(std::string_view target) const;

    /// Shorthand: convert and return just the value.
    [[nodiscard]] double get_value(std::string_view target) const;

    // -- Dimension checking --

    [[nodiscard]] bool conforms(const Unit& other) const;
    [[nodiscard]] bool conforms(const Quantity& other) const;

    // -- Arithmetic --

    /// Add quantities (must conform dimensionally; result in lhs unit).
    [[nodiscard]] Quantity operator+(const Quantity& rhs) const;
    /// Subtract quantities (must conform dimensionally; result in lhs unit).
    [[nodiscard]] Quantity operator-(const Quantity& rhs) const;
    /// Multiply quantities (any dimensions; result unit string is compound).
    [[nodiscard]] Quantity operator*(const Quantity& rhs) const;
    /// Divide quantities (any dimensions; result unit string is compound).
    [[nodiscard]] Quantity operator/(const Quantity& rhs) const;
    /// Scalar multiply.
    [[nodiscard]] Quantity operator*(double scale) const;
    /// Scalar divide.
    [[nodiscard]] Quantity operator/(double scale) const;
    /// Negate.
    [[nodiscard]] Quantity operator-() const;

    // -- Comparison (converts to common unit) --

    [[nodiscard]] bool operator==(const Quantity& rhs) const;
    [[nodiscard]] bool operator<(const Quantity& rhs) const;
    [[nodiscard]] bool operator>(const Quantity& rhs) const;
    [[nodiscard]] bool operator<=(const Quantity& rhs) const;
    [[nodiscard]] bool operator>=(const Quantity& rhs) const;

  private:
    Unit unit_;
};

/// Free function: convert a quantity to a different unit (backward compat).
/// @throws std::invalid_argument if the conversion is not supported.
[[nodiscard]] Quantity convert_quantity(const Quantity& q, std::string_view target_unit);

} // namespace casacore_mini
