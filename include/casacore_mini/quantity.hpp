// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/unit.hpp"

#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Physical quantity: numeric value + unit with arithmetic and conversion.
///
///
///
///
/// A Quantity pairs a `double` numeric value with a Unit, enabling
/// dimension-aware arithmetic and lossless unit conversion.  The design
/// mirrors casacore-original's `Quantum<double>` (also called
/// `Quantity`) while using a flat, non-template representation.
///
/// The public `value` member preserves backward compatibility with
/// code that constructs quantities as aggregates (`Quantity{1.5, "GHz"}`)
/// or accesses the raw number directly.
///
/// Unit conformance is checked lazily at conversion time rather than at
/// construction time, so constructing a Quantity with an unknown or
/// dimensionally inconsistent unit is not an error until a conversion or
/// comparison is attempted.
///
///
/// @par Example
/// @code{.cpp}
///   // Basic construction and conversion
///   Quantity freq(1.5, "GHz");
///   Quantity freq_hz = freq.convert("Hz");     // 1.5e9 Hz
///   double   hz_val  = freq.get_value("Hz");   // 1.5e9
///
///   // Arithmetic preserves units
///   Quantity bw(200, "MHz");
///   Quantity sum = freq + bw.convert("GHz");   // 1.7 GHz
///
///   // Conformance check
///   Quantity vel(30, "km/s");
///   assert(!freq.conforms(vel));   // Hz and km/s are incompatible
///
///   // Scalar scaling
///   Quantity double_freq = freq * 2.0;         // 3.0 GHz
/// @endcode
///
///
/// @par Motivation
/// Radio-astronomy data pipelines mix many physical quantities — frequencies,
/// velocities, angles, flux densities — each with its own unit system.
/// Wrapping a numeric value together with its unit prevents the common error
/// of silently applying a computation in the wrong unit and makes interface
/// contracts self-documenting.
///
/// @ingroup measures
/// @addtogroup measures
/// @{

/// A physical quantity: a double value paired with a Unit.
///
///
/// Supports conversion between any dimensionally compatible units, arithmetic
/// between quantities, and scalar multiplication/division.  Backward-compatible
/// with the original `Quantity{double, string}` aggregate -- `value` is a
/// public double member.
///
/// Addition and subtraction require dimensionally conformant units; the
/// right-hand operand is converted to the left-hand unit before the
/// operation.  Multiplication and division accept any unit combination and
/// produce a compound result unit string.
///
/// Comparison operators convert the right-hand operand to the left-hand
/// unit before comparing numeric values.  Comparing dimensionally
/// incompatible quantities throws `std::invalid_argument`.
///
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

    /// Return the unit string (e.g. "km/s").
    [[nodiscard]] const std::string& get_unit() const {
        return unit_.name();
    }

    /// Return the parsed Unit object.
    [[nodiscard]] const Unit& unit() const {
        return unit_;
    }

    /// Return the Unit's UnitVal (factor + dimensions).
    [[nodiscard]] const UnitVal& unit_value() const {
        return unit_.value();
    }

    // -- Conversion --

    /// Convert to a different unit (must be dimensionally conformant).
    ///
    ///
    /// Scales `value` by the ratio of the stored unit's SI factor
    /// to the target unit's SI factor, then wraps the result in a new
    /// Quantity with the target unit.  Dimension vectors must match exactly.
    ///
    ///
    /// @throws std::invalid_argument on dimension mismatch or unknown unit.
    [[nodiscard]] Quantity convert(const Unit& target) const;
    [[nodiscard]] Quantity convert(std::string_view target) const;

    /// Convert and return just the numeric value in the target unit.
    ///
    /// Equivalent to `convert(target).value`.
    ///
    /// @throws std::invalid_argument on dimension mismatch or unknown unit.
    [[nodiscard]] double get_value(std::string_view target) const;

    // -- Dimension checking --

    /// True if this quantity's dimensions conform to `other`'s unit.
    [[nodiscard]] bool conforms(const Unit& other) const;
    /// True if this quantity's dimensions conform to `other`'s unit.
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

    // -- Comparison (converts rhs to lhs unit before comparing) --

    [[nodiscard]] bool operator==(const Quantity& rhs) const;
    [[nodiscard]] bool operator<(const Quantity& rhs) const;
    [[nodiscard]] bool operator>(const Quantity& rhs) const;
    [[nodiscard]] bool operator<=(const Quantity& rhs) const;
    [[nodiscard]] bool operator>=(const Quantity& rhs) const;

  private:
    Unit unit_;
};

/// Convert a quantity to a different unit (free-function form for backward compatibility).
///
///
/// Equivalent to `q.convert(target_unit)`.  Provided for
/// compatibility with call sites that use the free-function spelling from
/// casacore-original's Quantum utilities.
///
///
/// @throws std::invalid_argument if the conversion is not supported.
[[nodiscard]] Quantity convert_quantity(const Quantity& q, std::string_view target_unit);

/// @}
} // namespace casacore_mini
