#include "casacore_mini/quantity.hpp"
#include "casacore_mini/unit.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double kTol = 1.0e-10;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    if (a == 0.0 && b == 0.0) {
        return true;
    }
    return std::abs(a - b) < tol * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

using namespace casacore_mini;

// =========================================================================
// UnitVal tests
// =========================================================================

bool test_unitval_default() {
    UnitVal uv;
    assert(uv.factor() == 1.0);
    for (int i = 0; i < UnitVal::kNDim; ++i) {
        assert(uv.dims()[static_cast<std::size_t>(i)] == 0);
    }
    return true;
}

bool test_unitval_factor() {
    UnitVal uv(42.0);
    assert(uv.factor() == 42.0);
    for (int i = 0; i < UnitVal::kNDim; ++i) {
        assert(uv.dims()[static_cast<std::size_t>(i)] == 0);
    }
    return true;
}

bool test_unitval_single_dim() {
    UnitVal uv(1.0, UnitVal::LENGTH);
    assert(uv.factor() == 1.0);
    assert(uv.dim(UnitVal::LENGTH) == 1);
    assert(uv.dim(UnitVal::MASS) == 0);
    assert(uv.dim(UnitVal::TIME) == 0);
    return true;
}

bool test_unitval_multi_dim() {
    // m^2.s^-1 (kinematic viscosity)
    UnitVal::DimArray d{};
    d[0] = 2;  // LENGTH
    d[2] = -1; // TIME
    UnitVal uv(1.0, d);
    assert(uv.dim(UnitVal::LENGTH) == 2);
    assert(uv.dim(UnitVal::TIME) == -1);
    return true;
}

bool test_unitval_multiply() {
    UnitVal length(1.0, UnitVal::LENGTH);
    UnitVal time_inv(1.0, UnitVal::TIME);
    [[maybe_unused]] auto velocity = length / time_inv;
    assert(velocity.dim(UnitVal::LENGTH) == 1);
    assert(velocity.dim(UnitVal::TIME) == -1);
    assert(velocity.factor() == 1.0);

    // km * 1000
    UnitVal km(1000.0, UnitVal::LENGTH);
    UnitVal s(1.0, UnitVal::TIME);
    [[maybe_unused]] auto kms = km / s;
    assert(kms.factor() == 1000.0);
    assert(kms.dim(UnitVal::LENGTH) == 1);
    assert(kms.dim(UnitVal::TIME) == -1);
    return true;
}

bool test_unitval_pow() {
    UnitVal m(1.0, UnitVal::LENGTH);
    [[maybe_unused]] auto m3 = m.pow(3);
    assert(m3.dim(UnitVal::LENGTH) == 3);
    assert(m3.factor() == 1.0);

    UnitVal km(1000.0, UnitVal::LENGTH);
    [[maybe_unused]] auto km2 = km.pow(2);
    assert(near(km2.factor(), 1e6));
    assert(km2.dim(UnitVal::LENGTH) == 2);

    [[maybe_unused]] auto inv = m.pow(-2);
    assert(inv.dim(UnitVal::LENGTH) == -2);
    return true;
}

bool test_unitval_conforms() {
    UnitVal m(1.0, UnitVal::LENGTH);
    UnitVal km(1000.0, UnitVal::LENGTH);
    assert(m.conforms(km));

    UnitVal s(1.0, UnitVal::TIME);
    assert(!m.conforms(s));

    // m/s conforms with km/h
    [[maybe_unused]] auto ms = m / s;
    [[maybe_unused]] auto kmh = km / UnitVal(3600.0, UnitVal::TIME);
    assert(ms.conforms(kmh));
    return true;
}

bool test_unitval_named_constants() {
    assert(UnitVal::NODIM.dims() == UnitVal().dims());
    assert(UnitVal::LENGTH_DIM.dim(UnitVal::LENGTH) == 1);
    assert(UnitVal::MASS_DIM.dim(UnitVal::MASS) == 1);
    assert(UnitVal::TIME_DIM.dim(UnitVal::TIME) == 1);
    assert(UnitVal::CURRENT_DIM.dim(UnitVal::CURRENT) == 1);
    assert(UnitVal::TEMPERATURE_DIM.dim(UnitVal::TEMPERATURE) == 1);
    assert(UnitVal::INTENSITY_DIM.dim(UnitVal::INTENSITY) == 1);
    assert(UnitVal::MOLAR_DIM.dim(UnitVal::MOLAR) == 1);
    assert(UnitVal::ANGLE_DIM.dim(UnitVal::ANGLE) == 1);
    assert(UnitVal::SOLIDANGLE_DIM.dim(UnitVal::SOLIDANGLE) == 1);
    return true;
}

// =========================================================================
// UnitMap tests
// =========================================================================

bool test_unitmap_basic_lookup() {
    [[maybe_unused]] auto m = UnitMap::find("m");
    assert(m.has_value());
    assert(m->dim(UnitVal::LENGTH) == 1);
    assert(m->factor() == 1.0);

    [[maybe_unused]] auto kg = UnitMap::find("kg");
    assert(kg.has_value());
    assert(kg->dim(UnitVal::MASS) == 1);

    [[maybe_unused]] auto s = UnitMap::find("s");
    assert(s.has_value());
    assert(s->dim(UnitVal::TIME) == 1);

    [[maybe_unused]] auto rad = UnitMap::find("rad");
    assert(rad.has_value());
    assert(rad->dim(UnitVal::ANGLE) == 1);
    return true;
}

bool test_unitmap_derived() {
    [[maybe_unused]] auto hz = UnitMap::find("Hz");
    assert(hz.has_value());
    assert(hz->dim(UnitVal::TIME) == -1);

    [[maybe_unused]] auto jy = UnitMap::find("Jy");
    assert(jy.has_value());
    assert(near(jy->factor(), 1e-26));
    assert(jy->dim(UnitVal::MASS) == 1);
    assert(jy->dim(UnitVal::TIME) == -2);

    [[maybe_unused]] auto pc = UnitMap::find("pc");
    assert(pc.has_value());
    assert(pc->dim(UnitVal::LENGTH) == 1);
    // pc ~ 3.086e16 m
    assert(near(pc->factor(), 3.0857e16, 1e-3));
    return true;
}

bool test_unitmap_prefix_lookup() {
    [[maybe_unused]] auto k = UnitMap::find_prefix("k");
    assert(k.has_value());
    assert(*k == 1e3);

    [[maybe_unused]] auto da = UnitMap::find_prefix("da");
    assert(da.has_value());
    assert(*da == 10.0);

    [[maybe_unused]] auto u = UnitMap::find_prefix("u");
    assert(u.has_value());
    assert(*u == 1e-6);

    [[maybe_unused]] auto nope = UnitMap::find_prefix("x");
    assert(!nope.has_value());
    return true;
}

bool test_unitmap_user_defined() {
    UnitMap::clear_user();

    auto bloop = UnitMap::find("bloop");
    assert(!bloop.has_value());

    UnitMap::define("bloop", UnitVal(42.0, UnitVal::LENGTH));
    bloop = UnitMap::find("bloop");
    assert(bloop.has_value());
    assert(bloop->factor() == 42.0);
    assert(bloop->dim(UnitVal::LENGTH) == 1);

    UnitMap::clear_user();
    bloop = UnitMap::find("bloop");
    assert(!bloop.has_value());
    return true;
}

bool test_unitmap_unknown() {
    [[maybe_unused]] auto nope = UnitMap::find("furlongs");
    assert(!nope.has_value());
    return true;
}

// =========================================================================
// Unit parsing tests
// =========================================================================

bool test_unit_simple() {
    Unit m("m");
    assert(m.defined());
    assert(m.value().dim(UnitVal::LENGTH) == 1);
    assert(m.value().factor() == 1.0);

    Unit kg("kg");
    assert(kg.defined());
    assert(kg.value().dim(UnitVal::MASS) == 1);

    Unit deg("deg");
    assert(deg.defined());
    assert(deg.value().dim(UnitVal::ANGLE) == 1);
    assert(near(deg.value().factor(), M_PI / 180.0));
    return true;
}

bool test_unit_prefixed() {
    Unit km("km");
    assert(km.defined());
    assert(km.value().dim(UnitVal::LENGTH) == 1);
    assert(near(km.value().factor(), 1000.0));

    Unit mhz("MHz");
    assert(mhz.defined());
    assert(mhz.value().dim(UnitVal::TIME) == -1);
    assert(near(mhz.value().factor(), 1e6));

    Unit ujy("uJy");
    assert(ujy.defined());
    assert(near(ujy.value().factor(), 1e-32));

    Unit mrad("mrad");
    assert(mrad.defined());
    assert(mrad.value().dim(UnitVal::ANGLE) == 1);
    assert(near(mrad.value().factor(), 1e-3));
    return true;
}

bool test_unit_compound() {
    Unit kms("km/s");
    assert(kms.defined());
    assert(kms.value().dim(UnitVal::LENGTH) == 1);
    assert(kms.value().dim(UnitVal::TIME) == -1);
    assert(near(kms.value().factor(), 1000.0));

    Unit acc("m.s-2");
    assert(acc.defined());
    assert(acc.value().dim(UnitVal::LENGTH) == 1);
    assert(acc.value().dim(UnitVal::TIME) == -2);

    Unit flux("W.m-2.Hz-1");
    assert(flux.defined());
    // W = kg.m^2.s^-3
    // W.m^-2.Hz^-1 = kg.m^0.s^-3.s^1 = kg.s^-2
    assert(flux.value().dim(UnitVal::MASS) == 1);
    assert(flux.value().dim(UnitVal::TIME) == -2);
    assert(flux.value().dim(UnitVal::LENGTH) == 0);
    return true;
}

bool test_unit_exponent_styles() {
    Unit m2a("m2");
    assert(m2a.defined());
    assert(m2a.value().dim(UnitVal::LENGTH) == 2);

    Unit m2b("m^2");
    assert(m2b.defined());
    assert(m2b.value().dim(UnitVal::LENGTH) == 2);

    Unit m2c("m**2");
    assert(m2c.defined());
    assert(m2c.value().dim(UnitVal::LENGTH) == 2);

    assert(near(m2a.value().factor(), m2b.value().factor()));
    assert(near(m2a.value().factor(), m2c.value().factor()));
    return true;
}

bool test_unit_parenthesized() {
    Unit u("km/(s.Mpc)");
    assert(u.defined());
    // km = 1e3 m, s = 1 s, Mpc = 1e6 * pc m
    // dims: LENGTH/TIME/LENGTH = TIME^-1
    // Actually: km/(s.Mpc) = km / s / Mpc
    // LENGTH^1 / TIME / LENGTH = TIME^-1
    assert(u.value().dim(UnitVal::TIME) == -1);
    assert(u.value().dim(UnitVal::LENGTH) == 0);
    return true;
}

bool test_unit_conformance() {
    Unit km("km");
    Unit m("m");
    assert(km.conforms(m));

    Unit s("s");
    assert(!km.conforms(s));

    Unit kms("km/s");
    Unit ms("m/s");
    assert(kms.conforms(ms));

    Unit hz("Hz");
    assert(!kms.conforms(hz));
    return true;
}

bool test_unit_empty() {
    Unit empty;
    assert(empty.empty());
    assert(empty.defined());
    assert(empty.value().factor() == 1.0);
    return true;
}

bool test_unit_unknown() {
    Unit u("furlongs");
    assert(!u.defined());
    assert(u.name() == "furlongs");
    return true;
}

bool test_unit_dimensionless() {
    Unit pct("%");
    assert(pct.defined());
    assert(near(pct.value().factor(), 0.01));

    Unit beam("beam");
    assert(beam.defined());
    assert(beam.value().factor() == 1.0);
    // All dims should be 0.
    for (int i = 0; i < UnitVal::kNDim; ++i) {
        assert(beam.value().dims()[static_cast<std::size_t>(i)] == 0);
    }
    return true;
}

bool test_unit_astronomical() {
    Unit au("AU");
    assert(au.defined());
    assert(au.value().dim(UnitVal::LENGTH) == 1);
    assert(near(au.value().factor(), 299792458.0 * 499.0047837));

    Unit pc("pc");
    assert(pc.defined());
    assert(pc.value().dim(UnitVal::LENGTH) == 1);

    Unit ly("ly");
    assert(ly.defined());
    assert(near(ly.value().factor(), 9.46073047e+15));

    Unit jy("Jy");
    assert(jy.defined());
    assert(near(jy.value().factor(), 1e-26));
    return true;
}

// Test that all predefined simple units parse without error.
bool test_unit_all_predefined_parse() {
    std::vector<std::string> names = {
        "m",     "kg",    "s",        "A",      "K",       "cd",    "mol",    "rad",     "sr",
        "g",     "_",     "$",        "%",      "%%",      "Hz",    "Bq",     "N",       "J",
        "W",     "Pa",    "C",        "V",      "F",       "Ohm",   "S",      "Wb",      "H",
        "T",     "lm",    "lx",       "Gy",     "Sv",      "deg",   "arcmin", "arcsec",  "as",
        "min",   "h",     "d",        "a",      "yr",      "cy",    "L",      "l",       "t",
        "Jy",    "FU",    "fu",       "WU",     "AU",      "UA",    "AE",     "pc",      "ly",
        "S0",    "M0",    "Angstrom", "in",     "ft",      "yd",    "mile",   "n_mile",  "fur",
        "lb",    "oz",    "cwt",      "u",      "CM",      "atm",   "bar",    "Torr",    "mHg",
        "ata",   "eV",    "erg",      "cal",    "Cal",     "Btu",   "dyn",    "hp",      "Gal",
        "St",    "kn",    "Ah",       "abA",    "abC",     "abF",   "abH",    "abOhm",   "abV",
        "statA", "statC", "statF",    "statH",  "statOhm", "statV", "debye",  "G",       "Mx",
        "Oe",    "Gb",    "sb",       "R",      "gal",     "USgal", "fl_oz",  "USfl_oz", "beam",
        "pixel", "count", "adu",      "lambda",
    };
    int ok = 0;
    for (const auto& n : names) {
        Unit u(n);
        if (!u.defined()) {
            std::cout << "  FAIL: predefined unit \"" << n << "\" not parseable\n";
            return false;
        }
        ++ok;
    }
    std::cout << "  (" << ok << " predefined units verified) ";
    return true;
}

// Test prefixed variants.
bool test_unit_prefixed_variants() {
    std::vector<std::pair<std::string, double>> cases = {
        {"km", 1e3},           {"cm", 1e-2}, {"mm", 1e-3}, {"um", 1e-6},  {"nm", 1e-9},
        {"kHz", 1e3},          {"MHz", 1e6}, {"GHz", 1e9}, {"THz", 1e12}, {"mJy", 1e-3 * 1e-26},
        {"uJy", 1e-6 * 1e-26}, {"kpc", 1e3}, // factor relative to pc
        {"Mpc", 1e6},
    };
    for (const auto& [name, expected_ratio] : cases) {
        Unit u(name);
        if (!u.defined()) {
            std::cout << "  FAIL: \"" << name << "\" not parseable\n";
            return false;
        }
        // For kpc/Mpc, check that factor = expected_ratio * pc_factor.
        if (name == "kpc" || name == "Mpc") {
            [[maybe_unused]] auto pc = UnitMap::find("pc");
            assert(pc.has_value());
            assert(near(u.value().factor(), expected_ratio * pc->factor(), 1e-6));
        }
    }
    return true;
}

// =========================================================================
// Quantity tests
// =========================================================================

bool test_quantity_construction() {
    Quantity q(180.0, "deg");
    assert(q.value == 180.0);
    assert(q.get_unit() == "deg");
    assert(q.unit().defined());

    Quantity zero;
    assert(zero.value == 0.0);
    return true;
}

bool test_quantity_conversion_basic() {
    Quantity deg180(180.0, "deg");
    auto rad = deg180.convert("rad");
    assert(near(rad.value, M_PI));
    assert(rad.get_unit() == "rad");

    auto back = rad.convert("deg");
    assert(near(back.value, 180.0));
    return true;
}

bool test_quantity_conversion_astro() {
    // 1 AU in meters.
    Quantity au(1.0, "AU");
    auto m = au.convert("m");
    assert(near(m.value, 299792458.0 * 499.0047837));

    // 1 pc in meters.
    Quantity pc(1.0, "pc");
    auto pm = pc.convert("m");
    assert(near(pm.value, 3.0857e16, 1e-3));

    // 1 Jy.
    Quantity jy(1.0, "Jy");
    assert(near(jy.unit_value().factor(), 1e-26));
    return true;
}

bool test_quantity_get_value() {
    Quantity deg(180.0, "deg");
    [[maybe_unused]] double rad = deg.get_value("rad");
    assert(near(rad, M_PI));
    return true;
}

bool test_quantity_conversion_fail() {
    Quantity q(1.0, "m");
    try {
        (void)q.convert("s");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { /* expected */
    }

    // Unknown source.
    Quantity unknown(1.0, "furlongs");
    try {
        (void)unknown.convert("m");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { /* expected */
    }

    // Unknown target.
    Quantity good(1.0, "m");
    try {
        (void)good.convert("furlongs");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { /* expected */
    }
    return true;
}

bool test_quantity_arithmetic_add_sub() {
    Quantity a(5.0, "km");
    Quantity b(3000.0, "m");
    auto sum = a + b;
    assert(near(sum.value, 8.0));
    assert(sum.get_unit() == "km");

    auto diff = a - b;
    assert(near(diff.value, 2.0));
    return true;
}

bool test_quantity_arithmetic_mul_div() {
    Quantity dist(100.0, "km");
    Quantity time(2.0, "h");
    auto speed = dist / time;
    assert(near(speed.value, 50.0));
    return true;
}

bool test_quantity_scalar_ops() {
    Quantity q(10.0, "m");
    auto doubled = q * 2.0;
    assert(near(doubled.value, 20.0));
    assert(doubled.get_unit() == "m");

    auto halved = q / 2.0;
    assert(near(halved.value, 5.0));
    return true;
}

bool test_quantity_negate() {
    Quantity q(5.0, "deg");
    auto neg = -q;
    assert(near(neg.value, -5.0));
    assert(neg.get_unit() == "deg");
    return true;
}

bool test_quantity_comparison() {
    Quantity a(1.0, "km");
    Quantity b(1000.0, "m");
    assert(a == b);

    Quantity c(999.0, "m");
    assert(c < a);
    assert(a > c);
    assert(c <= a);
    assert(a >= c);
    assert(a <= b);
    assert(a >= b);
    return true;
}

bool test_quantity_round_trips() {
    // rad -> deg -> rad
    Quantity q(3.14159, "rad");
    auto deg = q.convert("deg");
    auto back = deg.convert("rad");
    assert(near(back.value, q.value));

    // s -> min -> h -> d -> s
    Quantity sec(86400.0, "s");
    auto minutes = sec.convert("min");
    auto hours = minutes.convert("h");
    auto days = hours.convert("d");
    auto back_s = days.convert("s");
    assert(near(back_s.value, 86400.0));

    // m -> km -> m
    Quantity meters(1234.0, "m");
    auto km = meters.convert("km");
    auto back_m = km.convert("m");
    assert(near(back_m.value, 1234.0));
    return true;
}

bool test_quantity_astro_conversions() {
    // deg/rad/arcmin/arcsec.
    {
        Quantity deg(1.0, "deg");
        auto arcmin = deg.convert("arcmin");
        assert(near(arcmin.value, 60.0));
        auto arcsec = deg.convert("arcsec");
        assert(near(arcsec.value, 3600.0));
    }

    // Time: s/min/h/d/a.
    {
        Quantity yr(1.0, "a");
        auto days = yr.convert("d");
        assert(near(days.value, 365.25));
        auto hours = yr.convert("h");
        assert(near(hours.value, 365.25 * 24.0));
    }

    // Frequency: Hz/kHz/MHz/GHz.
    {
        Quantity ghz(1.0, "GHz");
        auto mhz = ghz.convert("MHz");
        assert(near(mhz.value, 1000.0));
        auto khz = ghz.convert("kHz");
        assert(near(khz.value, 1e6));
        auto hz = ghz.convert("Hz");
        assert(near(hz.value, 1e9));
    }

    // Length: m/km/AU/pc/ly.
    {
        Quantity pc(1.0, "pc");
        auto au = pc.convert("AU");
        // 1 pc = 1/arcsec AU = 206264.8... AU
        assert(near(au.value, 1.0 / (M_PI / 180.0 / 3600.0), 1e-3));
    }
    return true;
}

bool test_quantity_conforms() {
    Quantity a(1.0, "km");
    Quantity b(1.0, "m");
    assert(a.conforms(b));

    Quantity c(1.0, "s");
    assert(!a.conforms(c));

    Unit hz("Hz");
    assert(!a.conforms(hz));
    assert(c.conforms(Unit("min")));
    return true;
}

// =========================================================================
// Backward compatibility
// =========================================================================

bool test_backward_compat_convert_quantity() {
    Quantity q(M_PI, "rad");
    auto r = convert_quantity(q, "deg");
    assert(near(r.value, 180.0));
    assert(r.get_unit() == "deg");
    return true;
}

bool test_backward_compat_brace_init() {
    // Old style: Quantity{1.0, "m"}
    Quantity q{1.0, "m"};
    assert(q.value == 1.0);
    assert(q.get_unit() == "m");
    return true;
}

bool test_backward_compat_unknown_unit_convert() {
    // Old behavior: constructing with unknown unit is OK,
    // but converting throws.
    Quantity q(1.0, "furlongs");
    assert(!q.unit().defined());
    try {
        (void)convert_quantity(q, "m");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { /* expected */
    }
    return true;
}

} // namespace

int main() {
    int failures = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "  " << name << " ... ";
        try {
            if (fn()) {
                std::cout << "PASS\n";
            } else {
                std::cout << "FAIL\n";
                ++failures;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << "\n";
            ++failures;
        }
    };

    std::cout << "unit_test\n";

    std::cout << "\n--- UnitVal ---\n";
    run("default_construction", test_unitval_default);
    run("factor_construction", test_unitval_factor);
    run("single_dim", test_unitval_single_dim);
    run("multi_dim", test_unitval_multi_dim);
    run("multiply_divide", test_unitval_multiply);
    run("power", test_unitval_pow);
    run("conformance", test_unitval_conforms);
    run("named_constants", test_unitval_named_constants);

    std::cout << "\n--- UnitMap ---\n";
    run("basic_lookup", test_unitmap_basic_lookup);
    run("derived_units", test_unitmap_derived);
    run("prefix_lookup", test_unitmap_prefix_lookup);
    run("user_defined", test_unitmap_user_defined);
    run("unknown", test_unitmap_unknown);

    std::cout << "\n--- Unit parsing ---\n";
    run("simple_units", test_unit_simple);
    run("prefixed_units", test_unit_prefixed);
    run("compound_units", test_unit_compound);
    run("exponent_styles", test_unit_exponent_styles);
    run("parenthesized", test_unit_parenthesized);
    run("conformance", test_unit_conformance);
    run("empty", test_unit_empty);
    run("unknown", test_unit_unknown);
    run("dimensionless", test_unit_dimensionless);
    run("astronomical", test_unit_astronomical);
    run("all_predefined_parse", test_unit_all_predefined_parse);
    run("prefixed_variants", test_unit_prefixed_variants);

    std::cout << "\n--- Quantity ---\n";
    run("construction", test_quantity_construction);
    run("conversion_basic", test_quantity_conversion_basic);
    run("conversion_astro", test_quantity_conversion_astro);
    run("get_value", test_quantity_get_value);
    run("conversion_fail", test_quantity_conversion_fail);
    run("arithmetic_add_sub", test_quantity_arithmetic_add_sub);
    run("arithmetic_mul_div", test_quantity_arithmetic_mul_div);
    run("scalar_ops", test_quantity_scalar_ops);
    run("negate", test_quantity_negate);
    run("comparison", test_quantity_comparison);
    run("round_trips", test_quantity_round_trips);
    run("astro_conversions", test_quantity_astro_conversions);
    run("conforms", test_quantity_conforms);

    std::cout << "\n--- Backward compatibility ---\n";
    run("convert_quantity", test_backward_compat_convert_quantity);
    run("brace_init", test_backward_compat_brace_init);
    run("unknown_unit_convert", test_backward_compat_unknown_unit_convert);

    std::cout << "\n";
    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
