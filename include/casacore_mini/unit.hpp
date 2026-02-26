#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace casacore_mini {

/// @file
/// @brief Unit system: UnitVal (dimension+factor), Unit (parsed string),
///        UnitMap (predefined unit registry).

// ---------------------------------------------------------------------------
// UnitVal -- SI dimension vector + scale factor
// ---------------------------------------------------------------------------

/// Represents the dimensional decomposition and scale factor of a unit.
///
/// The 9 SI base dimensions match casacore-original's indices:
///   LENGTH(0), MASS(1), TIME(2), CURRENT(3), TEMPERATURE(4),
///   INTENSITY(5), MOLAR(6), ANGLE(7), SOLIDANGLE(8).
class UnitVal {
  public:
    static constexpr int kNDim = 9;

    enum Dim : int {
        LENGTH = 0,
        MASS = 1,
        TIME = 2,
        CURRENT = 3,
        TEMPERATURE = 4,
        INTENSITY = 5,
        MOLAR = 6,
        ANGLE = 7,
        SOLIDANGLE = 8
    };

    using DimArray = std::array<int8_t, kNDim>;

    /// Dimensionless with factor=1.
    UnitVal();
    /// Dimensionless with the given scale factor.
    explicit UnitVal(double factor);
    /// Single-dimension unit.
    UnitVal(double factor, Dim d);
    /// General: explicit factor and dimension vector.
    UnitVal(double factor, DimArray dims);

    [[nodiscard]] double factor() const {
        return factor_;
    }
    [[nodiscard]] int8_t dim(Dim d) const {
        return dims_[static_cast<std::size_t>(d)];
    }
    [[nodiscard]] const DimArray& dims() const {
        return dims_;
    }

    /// Multiply: factors multiply, dimension exponents add.
    [[nodiscard]] UnitVal operator*(const UnitVal& rhs) const;
    /// Divide: factors divide, dimension exponents subtract.
    [[nodiscard]] UnitVal operator/(const UnitVal& rhs) const;
    /// Power: factor^n, dims*n.
    [[nodiscard]] UnitVal pow(int n) const;

    /// Dimension conformance check (ignores factor).
    [[nodiscard]] bool conforms(const UnitVal& other) const;

    // Named dimension constants (factor=1, single dimension set to 1).
    static const UnitVal NODIM;
    static const UnitVal LENGTH_DIM;
    static const UnitVal MASS_DIM;
    static const UnitVal TIME_DIM;
    static const UnitVal CURRENT_DIM;
    static const UnitVal TEMPERATURE_DIM;
    static const UnitVal INTENSITY_DIM;
    static const UnitVal MOLAR_DIM;
    static const UnitVal ANGLE_DIM;
    static const UnitVal SOLIDANGLE_DIM;

  private:
    double factor_ = 1.0;
    DimArray dims_{};
};

// ---------------------------------------------------------------------------
// Unit -- lightweight wrapper: unit string + cached UnitVal
// ---------------------------------------------------------------------------

/// A unit string (e.g. "km/s", "Jy.beam-1") with a cached parsed UnitVal.
///
/// Construction never throws. If the string cannot be parsed (unknown unit),
/// `defined()` returns false and conversion will fail at use time.
/// Supports:
///   - Predefined units from UnitMap
///   - SI prefixes (24 standard prefixes)
///   - Compound separators: `.`, `*`, ` ` (multiply) and `/` (divide)
///   - Exponents: trailing digits, `^N`, `**N`
///   - Parenthesized groups: `km/(s.Mpc)`
class Unit {
  public:
    /// Empty (dimensionless).
    Unit();
    /// Parse the given unit string.  Never throws.
    explicit Unit(std::string name);
    /// Parse from string_view.
    explicit Unit(std::string_view name);
    /// Parse from C string literal.
    explicit Unit(const char* name); // NOLINT(google-explicit-constructor)

    [[nodiscard]] const std::string& name() const {
        return name_;
    }
    [[nodiscard]] const UnitVal& value() const {
        return val_;
    }

    [[nodiscard]] bool empty() const {
        return name_.empty();
    }

    /// True if the unit string was successfully parsed.
    [[nodiscard]] bool defined() const {
        return defined_;
    }

    /// Dimension conformance check (delegates to UnitVal::conforms).
    [[nodiscard]] bool conforms(const Unit& other) const;

  private:
    std::string name_;
    UnitVal val_;
    bool defined_ = true;
};

// ---------------------------------------------------------------------------
// UnitMap -- static registry of predefined units + prefixes
// ---------------------------------------------------------------------------

/// Static registry of predefined units.
///
/// Provides ~200 predefined astronomical/SI/customary units and 24 SI
/// prefixes matching casacore-original's unit name strings exactly.
class UnitMap {
  public:
    /// Look up a simple (non-compound) unit name.
    /// Returns nullopt if the name is not a known unit.
    [[nodiscard]] static std::optional<UnitVal> find(std::string_view name);

    /// Look up an SI prefix factor (e.g. "k" -> 1e3, "da" -> 10).
    [[nodiscard]] static std::optional<double> find_prefix(std::string_view name);

    /// Register a user-defined unit.
    static void define(std::string name, UnitVal val);

    /// Clear all user-defined units.
    static void clear_user();
};

/// Parse a compound unit string into a UnitVal.
/// @throws std::invalid_argument if the string contains unknown units.
[[nodiscard]] UnitVal parse_unit(std::string_view str);

/// Try to parse a compound unit string.  Returns nullopt on failure.
[[nodiscard]] std::optional<UnitVal> try_parse_unit(std::string_view str);

} // namespace casacore_mini
