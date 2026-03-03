// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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
///
///
///
/// 
/// The unit system provides three cooperating types:
///
///   - `UnitVal`: a scale factor and an 9-element SI dimension vector.
///     Arithmetic on UnitVal values combines factors and dimension exponents,
///     enabling conformance checks and conversion ratios.
///
///   - `Unit`: a lightweight wrapper around a unit name string.
///     Parsing is done eagerly on construction by walking the UnitMap registry
///     and resolving SI prefixes, compound separators, and exponents.
///     Construction never throws; unknown units are marked via `defined()`.
///
///   - `UnitMap`: a static registry of approximately 200 predefined
///     astronomical, SI, and customary units together with 24 SI prefixes.
///     User-defined units may be registered at runtime.
///
/// The unit name syntax is a superset of the FITS unit string convention:
/// atoms may be separated by `.`, `*`, or whitespace
/// (multiplication) and `/` (division).  Integer exponents are
/// written as a trailing digit string, `^N`, or `**N`.
/// Parenthesised groups such as `km/(s.Mpc)` are supported.
/// 
///
/// @par Example
/// @code{.cpp}
///   // Basic parsing and conformance
///   Unit km_per_s("km/s");
///   Unit m_per_s("m/s");
///   assert(km_per_s.conforms(m_per_s));       // same dimensions
///
///   // Conversion factor via UnitVal
///   double factor = km_per_s.value().factor()
///                 / m_per_s.value().factor();  // 1000.0
///
///   // Compound unit with SI prefix
///   Unit ghz("GHz");
///   assert(ghz.defined());
///   // ghz.value().factor() == 1e9
///
///   // Dimensionless check
///   Unit none("");
///   assert(none.conforms(Unit("1")));
///
///   // User-defined unit
///   UnitMap::define("beam", UnitVal(1.0));     // dimensionless
/// @endcode
/// 

// ---------------------------------------------------------------------------
// UnitVal -- SI dimension vector + scale factor
// ---------------------------------------------------------------------------

/// 
/// Dimensional decomposition and SI scale factor for a physical unit.
/// 
///
///
///
/// 
/// UnitVal stores a multiplicative scale factor (relative to SI base units)
/// and an 9-element signed exponent vector over the SI base dimensions.
/// The indices follow casacore-original's convention:
/// <dl>
///   <dt>0 LENGTH</dt>      <dd>metre (m)</dd>
///   <dt>1 MASS</dt>        <dd>kilogram (kg)</dd>
///   <dt>2 TIME</dt>        <dd>second (s)</dd>
///   <dt>3 CURRENT</dt>     <dd>ampere (A)</dd>
///   <dt>4 TEMPERATURE</dt> <dd>kelvin (K)</dd>
///   <dt>5 INTENSITY</dt>   <dd>candela (cd)</dd>
///   <dt>6 MOLAR</dt>       <dd>mole (mol)</dd>
///   <dt>7 ANGLE</dt>       <dd>radian (rad)</dd>
///   <dt>8 SOLIDANGLE</dt>  <dd>steradian (sr)</dd>
/// </dl>
///
/// Arithmetic operators combine UnitVals: multiplication adds dimension
/// exponents and multiplies factors; division subtracts exponents and
/// divides factors; `pow(n)` scales both.
///
/// Conformance (dimensional equivalence) is tested with
/// `conforms()`, which compares exponent vectors while ignoring
/// the scale factor.
/// 
///
/// @par Example
/// @code{.cpp}
///   UnitVal metre(1.0, UnitVal::LENGTH);
///   UnitVal second(1.0, UnitVal::TIME);
///   UnitVal velocity = metre / second;         // m/s: LENGTH=1, TIME=-1
///   UnitVal km_s(1e3, velocity.dims());        // km/s: same dims, factor=1000
///   assert(velocity.conforms(km_s));
///   double conv = km_s.factor() / velocity.factor();  // 1000.0
/// @endcode
/// 
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
    /// Single-dimension unit (exponent 1 in dimension `d`).
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
    /// Raise to integer power: factor^n, dims*n.
    [[nodiscard]] UnitVal pow(int n) const;

    /// Dimension conformance check (ignores factor; true iff all exponents match).
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

/// 
/// Parsed unit string with cached dimensional decomposition.
/// 
///
///
///
/// 
/// Unit is a lightweight value type that pairs a unit name string with the
/// UnitVal computed by parsing it.  Parsing is done once at construction.
/// Construction never throws; if the string cannot be resolved,
/// `defined()` returns false and any subsequent conversion attempt
/// will throw at use time.
///
/// Supported name syntax:
/// <dl>
///   <dt>Atom</dt>
///   <dd>A name from UnitMap (e.g. "Hz", "Jy", "arcsec") optionally
///       preceded by an SI prefix ("k", "G", "m", etc.).</dd>
///   <dt>Compound</dt>
///   <dd>Atoms joined by `.`, `*`, or whitespace
///       (multiply) and `/` (divide).</dd>
///   <dt>Exponent</dt>
///   <dd>Trailing digit string, `^N`, or `**N`
///       after an atom, e.g. `m2`, `s^-1`.</dd>
///   <dt>Groups</dt>
///   <dd>Parenthesised sub-expressions, e.g. `km/(s.Mpc)`.</dd>
/// </dl>
/// 
///
/// @par Example
/// @code{.cpp}
///   Unit freq("GHz");
///   assert(freq.defined());
///   assert(freq.value().dim(UnitVal::TIME) == -1);
///
///   Unit compound("Jy.beam-1");
///   assert(compound.defined());
///
///   Unit bad("frobnic");
///   assert(!bad.defined());    // unknown; no throw yet
///
///   // Conformance
///   assert(Unit("km/s").conforms(Unit("m/s")));
/// @endcode
/// 
class Unit {
  public:
    /// Empty (dimensionless).
    Unit();
    /// Parse the given unit string.  Never throws.
    explicit Unit(std::string name);
    /// Parse from string_view.
    explicit Unit(std::string_view name);
    /// Parse from C string literal (implicit conversion intentional for convenience).
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

/// 
/// Static registry of predefined astronomical, SI, and customary units.
/// 
///
///
///
/// 
/// UnitMap maintains two tables:
/// <dl>
///   <dt>Predefined units</dt>
///   <dd>Approximately 200 units matching casacore-original's name strings
///       exactly, including SI base units, derived units, astronomical units
///       (Jy, pc, au, arcsec, beam, etc.), and common customary units.</dd>
///   <dt>SI prefixes</dt>
///   <dd>24 standard prefixes from `y` (1e-24) to `Y`
///       (1e24), plus the two-character prefixes `da`, `h`.</dd>
/// </dl>
///
/// User-defined units registered via `define()` are stored in a
/// separate thread-local map and shadow predefined names.  They can be
/// cleared with `clear_user()`.
///
/// The `find()` and `find_prefix()` methods are called
/// internally by `parse_unit()` during Unit construction; direct
/// calls are rarely needed.
/// 
///
/// @par Example
/// @code{.cpp}
///   // Look up a known unit
///   auto opt = UnitMap::find("GHz");
///   assert(opt.has_value());
///   // opt->factor() == 1e9, TIME dim == -1
///
///   // Register a project-specific unit
///   UnitMap::define("SEFD", UnitVal(1e-26, UnitVal::DimArray{0,1,-2,0,0,0,0,0,0}));
///
///   // Clean up after tests
///   UnitMap::clear_user();
/// @endcode
/// 
class UnitMap {
  public:
    /// Look up a simple (non-compound) unit name.
    /// Returns nullopt if the name is not a known unit.
    [[nodiscard]] static std::optional<UnitVal> find(std::string_view name);

    /// Look up an SI prefix factor (e.g. "k" -> 1e3, "da" -> 10).
    [[nodiscard]] static std::optional<double> find_prefix(std::string_view name);

    /// Register a user-defined unit, shadowing any predefined unit of the same name.
    static void define(std::string name, UnitVal val);

    /// Clear all user-defined units, restoring only the predefined registry.
    static void clear_user();
};

/// Parse a compound unit string into a UnitVal.
///
/// 
/// Implements the full compound unit grammar: SI-prefixed atoms, multiply/
/// divide separators, exponents, and parenthesised groups.  Returns the
/// combined UnitVal on success.
/// 
///
/// @throws std::invalid_argument if the string contains unknown units.
[[nodiscard]] UnitVal parse_unit(std::string_view str);

/// Try to parse a compound unit string; return nullopt on failure instead of throwing.
///
/// 
/// Non-throwing variant of `parse_unit`.  Useful when testing
/// whether a string is a valid unit without branching on exceptions.
/// 
[[nodiscard]] std::optional<UnitVal> try_parse_unit(std::string_view str);

} // namespace casacore_mini
