// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/unit.hpp"

#include <cctype>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace casacore_mini {

// ===== UnitVal =============================================================

// Named dimension constants.
// NOLINTBEGIN(cert-err58-cpp)
const UnitVal UnitVal::NODIM{};
const UnitVal UnitVal::LENGTH_DIM{1.0, UnitVal::LENGTH};
const UnitVal UnitVal::MASS_DIM{1.0, UnitVal::MASS};
const UnitVal UnitVal::TIME_DIM{1.0, UnitVal::TIME};
const UnitVal UnitVal::CURRENT_DIM{1.0, UnitVal::CURRENT};
const UnitVal UnitVal::TEMPERATURE_DIM{1.0, UnitVal::TEMPERATURE};
const UnitVal UnitVal::INTENSITY_DIM{1.0, UnitVal::INTENSITY};
const UnitVal UnitVal::MOLAR_DIM{1.0, UnitVal::MOLAR};
const UnitVal UnitVal::ANGLE_DIM{1.0, UnitVal::ANGLE};
const UnitVal UnitVal::SOLIDANGLE_DIM{1.0, UnitVal::SOLIDANGLE};
// NOLINTEND(cert-err58-cpp)

UnitVal::UnitVal() = default;
UnitVal::UnitVal(double factor) : factor_(factor) {}
UnitVal::UnitVal(double factor, Dim d) : factor_(factor) {
    dims_[static_cast<std::size_t>(d)] = 1;
}
UnitVal::UnitVal(double factor, DimArray dims) : factor_(factor), dims_(dims) {}

UnitVal UnitVal::operator*(const UnitVal& rhs) const {
    DimArray d{};
    for (int i = 0; i < kNDim; ++i) {
        d[static_cast<std::size_t>(i)] = static_cast<int8_t>(
            dims_[static_cast<std::size_t>(i)] + rhs.dims_[static_cast<std::size_t>(i)]);
    }
    return {factor_ * rhs.factor_, d};
}

UnitVal UnitVal::operator/(const UnitVal& rhs) const {
    DimArray d{};
    for (int i = 0; i < kNDim; ++i) {
        d[static_cast<std::size_t>(i)] = static_cast<int8_t>(
            dims_[static_cast<std::size_t>(i)] - rhs.dims_[static_cast<std::size_t>(i)]);
    }
    return {factor_ / rhs.factor_, d};
}

UnitVal UnitVal::pow(int n) const {
    DimArray d{};
    for (int i = 0; i < kNDim; ++i) {
        d[static_cast<std::size_t>(i)] =
            static_cast<int8_t>(dims_[static_cast<std::size_t>(i)] * n);
    }
    return {std::pow(factor_, n), d};
}

bool UnitVal::conforms(const UnitVal& other) const {
    return dims_ == other.dims_;
}

// ===== UnitMap =============================================================

namespace {

// Mathematical constants matching casacore-original's C:: namespace.
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegree = kPi / 180.0;
constexpr double kArcmin = kDegree / 60.0;
constexpr double kArcsec = kDegree / 3600.0;
constexpr double kSquareDegree = kDegree * kDegree;
constexpr double kSquareArcmin = kArcmin * kArcmin;
constexpr double kSquareArcsec = kArcsec * kArcsec;
constexpr double kCLight = 299792458.0; // m/s

// Time constants.
constexpr double kMinute = 60.0;
constexpr double kHour = 3600.0;
constexpr double kDay = 86400.0;
constexpr double kYear = 24.0 * 3600.0 * 365.25;
constexpr double kCentury = 24.0 * 3600.0 * 36525.0;

// IAU constants.
constexpr double kIauTauA = 499.0047837; // light-time for 1 AU in seconds
constexpr double kIauK = 0.01720209895;  // Gaussian gravitational constant

struct PrefixEntry {
    std::string_view name;
    double factor;
};

// 24 SI prefixes.
constexpr PrefixEntry kPrefixes[] = {
    {"Q", 1e30},  {"R", 1e27},  {"Y", 1e24},  {"Z", 1e21},  {"E", 1e18},  {"P", 1e15},
    {"T", 1e12},  {"G", 1e9},   {"M", 1e6},   {"k", 1e3},   {"h", 1e2},   {"da", 1e1},
    {"d", 1e-1},  {"c", 1e-2},  {"m", 1e-3},  {"u", 1e-6},  {"n", 1e-9},  {"p", 1e-12},
    {"f", 1e-15}, {"a", 1e-18}, {"z", 1e-21}, {"y", 1e-24}, {"r", 1e-27}, {"q", 1e-30},
};

// Helper: build a UnitVal from factor and dimension enum.
UnitVal dim_val(double factor, UnitVal::Dim d) {
    return {factor, d};
}

// Helper for multi-dim construction.
UnitVal make_val(double factor, UnitVal::DimArray dims) {
    return {factor, dims};
}

// Build the predefined unit maps on first use.
struct UnitMaps {
    std::unordered_map<std::string, UnitVal> units;
    std::unordered_map<std::string, double> prefixes;
    std::unordered_map<std::string, UnitVal> user_units;

    UnitMaps() {
        init_prefixes();
        init_defining();
        init_si();
        init_customary();
    }

    void init_prefixes() {
        for (const auto& p : kPrefixes) {
            prefixes.emplace(std::string(p.name), p.factor);
        }
    }

    void init_defining() {
        // 9 SI base units.
        units["m"] = dim_val(1.0, UnitVal::LENGTH);
        units["kg"] = dim_val(1.0, UnitVal::MASS);
        units["s"] = dim_val(1.0, UnitVal::TIME);
        units["A"] = dim_val(1.0, UnitVal::CURRENT);
        units["K"] = dim_val(1.0, UnitVal::TEMPERATURE);
        units["cd"] = dim_val(1.0, UnitVal::INTENSITY);
        units["mol"] = dim_val(1.0, UnitVal::MOLAR);
        units["rad"] = dim_val(1.0, UnitVal::ANGLE);
        units["sr"] = dim_val(1.0, UnitVal::SOLIDANGLE);

        // Gram (sub-unit of kg).
        units["g"] = dim_val(0.001, UnitVal::MASS);

        // Undimensioned.
        units["_"] = UnitVal(1.0);
        units["$"] = UnitVal(1.0);
        units["%"] = UnitVal(0.01);
        units["%%"] = UnitVal(0.001);
    }

    void init_si() {
        // ----- Derived SI units (using dimension arithmetic) -----

        // Frequency: s^-1
        UnitVal per_s = dim_val(1.0, UnitVal::TIME).pow(-1);
        units["Hz"] = per_s;
        units["Bq"] = per_s;

        // Force: kg.m.s^-2
        UnitVal newton = dim_val(1.0, UnitVal::MASS) * dim_val(1.0, UnitVal::LENGTH) *
                         dim_val(1.0, UnitVal::TIME).pow(-2);
        units["N"] = newton;

        // Energy: N.m = kg.m^2.s^-2
        UnitVal joule = newton * dim_val(1.0, UnitVal::LENGTH);
        units["J"] = joule;

        // Power: J/s = kg.m^2.s^-3
        UnitVal watt = joule / dim_val(1.0, UnitVal::TIME);
        units["W"] = watt;

        // Pressure: N/m^2 = kg.m^-1.s^-2
        UnitVal pascal = newton / dim_val(1.0, UnitVal::LENGTH).pow(2);
        units["Pa"] = pascal;

        // Charge: A.s
        UnitVal coulomb = dim_val(1.0, UnitVal::CURRENT) * dim_val(1.0, UnitVal::TIME);
        units["C"] = coulomb;

        // Voltage: W/A = kg.m^2.s^-3.A^-1
        UnitVal volt = watt / dim_val(1.0, UnitVal::CURRENT);
        units["V"] = volt;

        // Capacitance: C/V = A^2.s^4.kg^-1.m^-2
        units["F"] = coulomb / volt;

        // Resistance: V/A = kg.m^2.s^-3.A^-2
        UnitVal ohm = volt / dim_val(1.0, UnitVal::CURRENT);
        units["Ohm"] = ohm;

        // Conductance: 1/Ohm
        units["S"] = UnitVal(1.0) / ohm;

        // Magnetic flux: V.s = kg.m^2.s^-2.A^-1
        UnitVal weber = volt * dim_val(1.0, UnitVal::TIME);
        units["Wb"] = weber;

        // Inductance: Wb/A = kg.m^2.s^-2.A^-2
        units["H"] = weber / dim_val(1.0, UnitVal::CURRENT);

        // Magnetic flux density: Wb/m^2 = kg.s^-2.A^-1
        units["T"] = weber / dim_val(1.0, UnitVal::LENGTH).pow(2);

        // Luminous flux: cd.sr
        UnitVal lumen = dim_val(1.0, UnitVal::INTENSITY) * dim_val(1.0, UnitVal::SOLIDANGLE);
        units["lm"] = lumen;

        // Illuminance: lm/m^2
        units["lx"] = lumen / dim_val(1.0, UnitVal::LENGTH).pow(2);

        // Absorbed dose: J/kg (dimensionally m^2.s^-2)
        UnitVal gray = joule / dim_val(1.0, UnitVal::MASS);
        units["Gy"] = gray;
        units["Sv"] = gray;

        // ----- Accepted non-SI units -----

        // Angle.
        units["deg"] = dim_val(kDegree, UnitVal::ANGLE);
        units["arcmin"] = dim_val(kArcmin, UnitVal::ANGLE);
        units["arcsec"] = dim_val(kArcsec, UnitVal::ANGLE);
        units["as"] = units["arcsec"];
        units["'"] = units["arcmin"];
        units["''"] = units["arcsec"];
        units["\""] = units["arcsec"];

        // Solid angle.
        units["sq_deg"] = dim_val(kSquareDegree, UnitVal::SOLIDANGLE);
        units["deg_2"] = units["sq_deg"];
        units["sq_arcmin"] = dim_val(kSquareArcmin, UnitVal::SOLIDANGLE);
        units["arcmin_2"] = units["sq_arcmin"];
        units["sq_arcsec"] = dim_val(kSquareArcsec, UnitVal::SOLIDANGLE);
        units["arcsec_2"] = units["sq_arcsec"];
        units["'_2"] = units["sq_arcmin"];
        units["''_2"] = units["sq_arcsec"];
        units["\"_2"] = units["sq_arcsec"];

        // Time.
        units["min"] = dim_val(kMinute, UnitVal::TIME);
        units["h"] = dim_val(kHour, UnitVal::TIME);
        units["d"] = dim_val(kDay, UnitVal::TIME);
        units["a"] = dim_val(kYear, UnitVal::TIME);
        units["yr"] = units["a"];
        units["cy"] = dim_val(kCentury, UnitVal::TIME);
        // Time separator aliases (casacore-original).
        units[":"] = units["h"];
        units["::"] = units["min"];
        units[":::"] = units["s"];

        // Volume.
        // L = dm^3 = 1e-3 m^3
        units["L"] = dim_val(1e-3, UnitVal::LENGTH).pow(1);
        // Actually L is a volume unit: 1e-3 m^3 = factor 1e-3 with LENGTH^3
        units["L"] = make_val(1e-3, {3, 0, 0, 0, 0, 0, 0, 0, 0});
        units["l"] = units["L"];

        // Mass.
        units["t"] = dim_val(1000.0, UnitVal::MASS);

        // ----- Astronomical units -----

        // Jansky: 1e-26 W/m^2/Hz = 1e-26 kg.s^-2
        units["Jy"] = make_val(1e-26, {0, 1, -2, 0, 0, 0, 0, 0, 0});

        // Flux unit aliases.
        units["FU"] = units["Jy"];
        units["fu"] = units["Jy"];
        units["WU"] = make_val(5e-29, {0, 1, -2, 0, 0, 0, 0, 0, 0}); // 5 mJy

        // Astronomical Unit.
        double au_m = kCLight * kIauTauA;
        units["AU"] = dim_val(au_m, UnitVal::LENGTH);
        units["UA"] = units["AU"];
        units["AE"] = units["AU"];

        // Parsec: 1/arcsec AU = AU / arcsec(rad).
        double pc_m = au_m / kArcsec;
        units["pc"] = dim_val(pc_m, UnitVal::LENGTH);

        // Light year.
        units["ly"] = dim_val(9.46073047e+15, UnitVal::LENGTH);

        // Solar mass: IAU_k^2 / G * AU^3/d^2 ... use computed value.
        // S0 = IAU_k^2 / 6.67259e-11 in units of (AU^3/d^2)/(m^3/kg/s^2).
        // For simplicity, store the computed SI mass in kg.
        double s0_kg = (kIauK * kIauK * au_m * au_m * au_m) / (6.67259e-11 * kDay * kDay);
        units["S0"] = dim_val(s0_kg, UnitVal::MASS);
        units["M0"] = units["S0"];
    }

    void init_customary() {
        // Angstrom.
        units["Angstrom"] = dim_val(1e-10, UnitVal::LENGTH);

        // Length conversions.
        constexpr double kInchM = 2.54e-2;
        constexpr double kFootM = 12.0 * kInchM;
        constexpr double kYardM = 3.0 * kFootM;
        constexpr double kMileM = 5280.0 * kFootM;
        constexpr double kNMileCm = 6080.0 * 12.0 * 2.54;

        units["in"] = dim_val(kInchM, UnitVal::LENGTH);
        units["ft"] = dim_val(kFootM, UnitVal::LENGTH);
        units["yd"] = dim_val(kYardM, UnitVal::LENGTH);
        units["mile"] = dim_val(kMileM, UnitVal::LENGTH);
        units["n_mile"] = dim_val(kNMileCm * 0.01, UnitVal::LENGTH);
        units["fur"] = dim_val(220.0 * 3.0 * 12.0 * 2.54 * 0.01, UnitVal::LENGTH);

        // Area.
        units["ac"] = make_val(4.0 * 40.0 * 16.5 * 12.0 * 2.54e-2 * 16.5 * 12.0 * 2.54e-2,
                               {2, 0, 0, 0, 0, 0, 0, 0, 0});
        units["ha"] = make_val(1e4, {2, 0, 0, 0, 0, 0, 0, 0, 0});

        // Mass.
        constexpr double kPoundKg = 0.45359237;
        units["lb"] = dim_val(kPoundKg, UnitVal::MASS);
        units["oz"] = dim_val(kPoundKg / 16.0, UnitVal::MASS);
        units["cwt"] = dim_val(4.0 * 2.0 * 14.0 * kPoundKg, UnitVal::MASS);
        units["u"] = dim_val(1.661e-27, UnitVal::MASS);
        units["CM"] = dim_val(1e-3 / 5.0, UnitVal::MASS);

        // Pressure.
        units["atm"] = make_val(1.01325e+5, {-1, 1, -2, 0, 0, 0, 0, 0, 0});
        units["bar"] = make_val(1e+5, {-1, 1, -2, 0, 0, 0, 0, 0, 0});
        units["Torr"] = make_val((1.0 / 760.0) * 1.01325e+5, {-1, 1, -2, 0, 0, 0, 0, 0, 0});
        units["mHg"] = make_val(13.5951 * 9.80665e+3, {-1, 1, -2, 0, 0, 0, 0, 0, 0});
        units["ata"] = make_val(9.80665e+4, {-1, 1, -2, 0, 0, 0, 0, 0, 0});

        // Energy.
        units["eV"] = make_val(1.60217733e-19, {2, 1, -2, 0, 0, 0, 0, 0, 0});
        units["erg"] = make_val(1e-7, {2, 1, -2, 0, 0, 0, 0, 0, 0});
        units["cal"] = make_val(4.1868, {2, 1, -2, 0, 0, 0, 0, 0, 0});
        units["Cal"] = make_val(4186.8, {2, 1, -2, 0, 0, 0, 0, 0, 0});
        units["Btu"] = make_val(1055.056, {2, 1, -2, 0, 0, 0, 0, 0, 0});

        // Force.
        units["dyn"] = make_val(1e-5, {1, 1, -2, 0, 0, 0, 0, 0, 0});

        // Power.
        units["hp"] = make_val(745.7, {2, 1, -3, 0, 0, 0, 0, 0, 0});

        // Acceleration.
        units["Gal"] = make_val(0.01, {1, 0, -2, 0, 0, 0, 0, 0, 0});

        // Viscosity.
        units["St"] = make_val(1e-4, {2, 0, -1, 0, 0, 0, 0, 0, 0});

        // Speed.
        units["kn"] = make_val(kNMileCm * 0.01 / kHour, {1, 0, -1, 0, 0, 0, 0, 0, 0});

        // Charge/current.
        units["Ah"] = make_val(kHour, {0, 0, 1, 1, 0, 0, 0, 0, 0});

        // CGS electromagnetic units.
        units["abA"] = dim_val(10.0, UnitVal::CURRENT);
        units["abC"] = make_val(10.0, {0, 0, 1, 1, 0, 0, 0, 0, 0});
        units["abF"] = make_val(1e+9, {-2, -1, 4, 2, 0, 0, 0, 0, 0});
        units["abH"] = make_val(1e-9, {2, 1, -2, -2, 0, 0, 0, 0, 0});
        units["abOhm"] = make_val(1e-9, {2, 1, -3, -2, 0, 0, 0, 0, 0});
        units["abV"] = make_val(1e-8, {2, 1, -3, -1, 0, 0, 0, 0, 0});

        units["statA"] = dim_val(0.1 / kCLight, UnitVal::CURRENT);
        units["statC"] = make_val(0.1 / kCLight, {0, 0, 1, 1, 0, 0, 0, 0, 0});
        units["statF"] = make_val(1.0 / (3.0e+3 * kCLight), {-2, -1, 4, 2, 0, 0, 0, 0, 0});
        units["statH"] = make_val(3.0e+3 * kCLight, {2, 1, -2, -2, 0, 0, 0, 0, 0});
        units["statOhm"] = make_val(3.0e+3 * kCLight, {2, 1, -3, -2, 0, 0, 0, 0, 0});
        units["statV"] = make_val(kCLight * 1e-6, {2, 1, -3, -1, 0, 0, 0, 0, 0});

        // Debye: 10e-18 statC.cm = 10e-18 * (0.1/c) * 0.01 A.s.m
        units["debye"] = make_val(1e-18 * (0.1 / kCLight) * 0.01, {1, 0, 1, 1, 0, 0, 0, 0, 0});

        // Magnetic (CGS).
        units["G"] = make_val(1e-4, {0, 1, -2, -1, 0, 0, 0, 0, 0});
        units["Mx"] = make_val(1e-8, {2, 1, -2, -1, 0, 0, 0, 0, 0});
        units["Oe"] = make_val(1000.0 / (4.0 * kPi), {-1, 0, 0, 1, 0, 0, 0, 0, 0});
        units["Gb"] = dim_val(10.0 / (4.0 * kPi), UnitVal::CURRENT);

        // Photometry.
        units["sb"] = make_val(1e4, {-2, 0, 0, 0, 0, 1, 0, 0, 0});
        units["R"] = make_val(2.58e-4, {0, -1, 1, 1, 0, 0, 0, 0, 0});

        // Volume.
        constexpr double kGalImpCm3 = 277.4193 * 2.54 * 2.54 * 2.54;
        constexpr double kGalUsCm3 = 231.0 * 2.54 * 2.54 * 2.54;
        units["gal"] = make_val(kGalImpCm3 * 1e-6, {3, 0, 0, 0, 0, 0, 0, 0, 0});
        units["USgal"] = make_val(kGalUsCm3 * 1e-6, {3, 0, 0, 0, 0, 0, 0, 0, 0});
        units["fl_oz"] =
            make_val(kGalImpCm3 / (5.0 * 4.0 * 2.0 * 4.0) * 1e-6, {3, 0, 0, 0, 0, 0, 0, 0, 0});
        units["USfl_oz"] =
            make_val(kGalUsCm3 / (4.0 * 4.0 * 2.0 * 4.0) * 1e-6, {3, 0, 0, 0, 0, 0, 0, 0, 0});

        // Dimensionless pseudo-units.
        units["beam"] = UnitVal(1.0);
        units["pixel"] = UnitVal(1.0);
        units["count"] = UnitVal(1.0);
        units["adu"] = UnitVal(1.0);
        units["lambda"] = UnitVal(1.0);
    }
};

UnitMaps& get_maps() {
    static UnitMaps maps;
    return maps;
}

std::mutex& user_mutex() {
    static std::mutex mtx;
    return mtx;
}

} // anonymous namespace

std::optional<UnitVal> UnitMap::find(std::string_view name) {
    auto& maps = get_maps();

    // Check user-defined first.
    {
        std::lock_guard lock(user_mutex());
        std::string key(name);
        auto it = maps.user_units.find(key);
        if (it != maps.user_units.end()) {
            return it->second;
        }
    }

    // Check predefined.
    std::string key(name);
    auto it = maps.units.find(key);
    if (it != maps.units.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<double> UnitMap::find_prefix(std::string_view name) {
    auto& maps = get_maps();
    std::string key(name);
    auto it = maps.prefixes.find(key);
    if (it != maps.prefixes.end()) {
        return it->second;
    }
    return std::nullopt;
}

void UnitMap::define(std::string name, UnitVal val) {
    std::lock_guard lock(user_mutex());
    get_maps().user_units[std::move(name)] = val;
}

void UnitMap::clear_user() {
    std::lock_guard lock(user_mutex());
    get_maps().user_units.clear();
}

// ===== Unit string parser ==================================================

namespace {

// Is c a valid start character for a unit field?
bool is_unit_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '\'' || c == '"' ||
           c == '$' || c == '%' || c == ':';
}

// Is c a valid continuation character for a unit field?
bool is_unit_char(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '\'' || c == '"' ||
           c == '$' || c == '%' || c == ':' || c == '0';
}

// Parse an integer exponent from the current position.
// Recognizes: trailing digits, `^N`, `**N`, with optional sign.
int parse_exponent(std::string_view str, std::size_t& pos) {
    if (pos >= str.size()) {
        return 1;
    }

    // Skip ^ or ** prefix.
    if (str[pos] == '^') {
        ++pos;
    } else if (pos + 1 < str.size() && str[pos] == '*' && str[pos + 1] == '*') {
        pos += 2;
    } else if (std::isdigit(static_cast<unsigned char>(str[pos])) == 0 && str[pos] != '-' &&
               str[pos] != '+') {
        return 1;
    }

    int sign = 1;
    if (pos < str.size() && str[pos] == '-') {
        sign = -1;
        ++pos;
    } else if (pos < str.size() && str[pos] == '+') {
        ++pos;
    }

    int exp = 0;
    bool has_digit = false;
    while (pos < str.size() && std::isdigit(static_cast<unsigned char>(str[pos])) != 0) {
        exp = exp * 10 + (str[pos] - '0');
        ++pos;
        has_digit = true;
    }

    return has_digit ? sign * exp : 1;
}

// Try to resolve a unit name (possibly with prefix) to a UnitVal.
std::optional<UnitVal> resolve_field(std::string_view field) {
    // 1. Exact match.
    auto exact = UnitMap::find(field);
    if (exact) {
        return exact;
    }

    // 2. Single-char prefix + rest.
    if (field.size() >= 2) {
        auto pfx1 = UnitMap::find_prefix(field.substr(0, 1));
        if (pfx1) {
            auto base = UnitMap::find(field.substr(1));
            if (base) {
                return UnitVal(*pfx1 * base->factor(), base->dims());
            }
        }
    }

    // 3. Two-char prefix ("da") + rest.
    if (field.size() >= 3) {
        auto pfx2 = UnitMap::find_prefix(field.substr(0, 2));
        if (pfx2) {
            auto base = UnitMap::find(field.substr(2));
            if (base) {
                return UnitVal(*pfx2 * base->factor(), base->dims());
            }
        }
    }

    return std::nullopt;
}

// Forward declaration for recursive parsing.
UnitVal parse_compound(std::string_view str, std::size_t& pos);

// Parse a single token (field or parenthesized group).
UnitVal parse_atom(std::string_view str, std::size_t& pos) {
    if (pos >= str.size()) {
        return UnitVal(1.0);
    }

    if (str[pos] == '(') {
        // Find matching closing paren.
        ++pos;
        int depth = 1;
        std::size_t start = pos;
        while (pos < str.size() && depth > 0) {
            if (str[pos] == '(') {
                ++depth;
            } else if (str[pos] == ')') {
                --depth;
            }
            if (depth > 0) {
                ++pos;
            }
        }
        if (depth != 0) {
            throw std::invalid_argument("Unmatched '(' in unit string");
        }
        std::string_view inner = str.substr(start, pos - start);
        ++pos; // skip ')'

        // Parse the inner string.
        std::size_t inner_pos = 0;
        UnitVal result = parse_compound(inner, inner_pos);

        // Parse exponent after the closing paren.
        int exp = parse_exponent(str, pos);
        if (exp != 1) {
            result = result.pow(exp);
        }
        return result;
    }

    // Parse a field name.
    if (!is_unit_start(str[pos])) {
        throw std::invalid_argument(std::string("Unexpected character '") + str[pos] +
                                    "' in unit string \"" + std::string(str) + "\"");
    }

    std::size_t start = pos;
    ++pos;
    while (pos < str.size() && is_unit_char(str[pos])) {
        ++pos;
    }

    std::string_view field = str.substr(start, pos - start);

    // Parse exponent.
    int exp = parse_exponent(str, pos);

    auto resolved = resolve_field(field);
    if (!resolved) {
        throw std::invalid_argument("Unknown unit: \"" + std::string(field) + "\" in \"" +
                                    std::string(str) + "\"");
    }

    if (exp != 1) {
        return resolved->pow(exp);
    }
    return *resolved;
}

// Parse a compound unit expression.
UnitVal parse_compound(std::string_view str, std::size_t& pos) {
    UnitVal result(1.0);

    // Skip leading whitespace.
    while (pos < str.size() && str[pos] == ' ') {
        ++pos;
    }

    if (pos >= str.size()) {
        return result;
    }

    // Parse first atom.
    result = parse_atom(str, pos);

    // Parse remaining atoms with multiply/divide operators.
    while (pos < str.size()) {
        // Skip whitespace.
        while (pos < str.size() && str[pos] == ' ') {
            ++pos;
        }
        if (pos >= str.size()) {
            break;
        }

        char sep = str[pos];
        bool divide = false;

        if (sep == ')') {
            // End of parenthesized group.
            break;
        }

        if (sep == '/') {
            divide = true;
            ++pos;
        } else if (sep == '.' || (sep == '*' && (pos + 1 >= str.size() || str[pos + 1] != '*'))) {
            ++pos;
        } else if (!(is_unit_start(sep) || sep == '(')) {
            break;
        } else {
            // Implicit multiplication (space-separated).
        }

        UnitVal atom = parse_atom(str, pos);
        if (divide) {
            result = result / atom;
        } else {
            result = result * atom;
        }
    }

    return result;
}

} // anonymous namespace

UnitVal parse_unit(std::string_view str) {
    if (str.empty()) {
        return UnitVal(1.0);
    }
    std::size_t pos = 0;
    UnitVal result = parse_compound(str, pos);
    if (pos != str.size()) {
        throw std::invalid_argument("Trailing characters in unit string: \"" + std::string(str) +
                                    "\"");
    }
    return result;
}

std::optional<UnitVal> try_parse_unit(std::string_view str) {
    if (str.empty()) {
        return UnitVal(1.0);
    }
    try {
        std::size_t pos = 0;
        UnitVal result = parse_compound(str, pos);
        if (pos != str.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

// ===== Unit ================================================================

Unit::Unit() = default;

Unit::Unit(std::string name) : name_(std::move(name)) {
    if (!name_.empty()) {
        auto parsed = try_parse_unit(name_);
        if (parsed) {
            val_ = *parsed;
            defined_ = true;
        } else {
            defined_ = false;
        }
    }
}

Unit::Unit(std::string_view name) : Unit(std::string(name)) {}

Unit::Unit(const char* name) : Unit(std::string(name)) {}

bool Unit::conforms(const Unit& other) const {
    return val_.conforms(other.val_);
}

} // namespace casacore_mini
