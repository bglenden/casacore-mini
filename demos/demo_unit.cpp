// demo_unit.cpp -- Unit system showcase
//
// Demonstrates the Unit/UnitVal/UnitMap/Quantity classes:
//   - Unit parsing (simple, prefixed, compound)
//   - Quantity construction, conversion, and arithmetic
//   - Astronomical unit conversions
//   - SI prefix system

#include <casacore_mini/quantity.hpp>
#include <casacore_mini/unit.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

using namespace casacore_mini;

namespace {

constexpr double kTol = 1.0e-6;

bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

void demo_unit_basics() {
    std::cout << "\n=== Unit Basics ===\n";

    Unit kms("km/s");
    std::cout << "  Unit(\"km/s\") factor=" << kms.value().factor() << ", dims=[";
    for (int i = 0; i < UnitVal::kNDim; ++i) {
        if (i > 0) {
            std::cout << ",";
        }
        std::cout << static_cast<int>(kms.value().dims()[static_cast<std::size_t>(i)]);
    }
    std::cout << "]\n";
    assert(near(kms.value().factor(), 1000.0));
    assert(kms.value().dim(UnitVal::LENGTH) == 1);
    assert(kms.value().dim(UnitVal::TIME) == -1);

    Unit jy("Jy");
    std::cout << "  Unit(\"Jy\") factor=" << jy.value().factor() << " (MASS*TIME^-2)\n";
    assert(near(jy.value().factor(), 1e-26));

    Unit ms("m/s");
    std::cout << "  Conforms check: km/s ~ m/s -> " << (kms.conforms(ms) ? "true" : "false")
              << "\n";
    assert(kms.conforms(ms));

    Unit hz("Hz");
    std::cout << "  Conforms check: km/s ~ Hz -> " << (kms.conforms(hz) ? "true" : "false") << "\n";
    assert(!kms.conforms(hz));

    std::cout << "  [OK] Unit basics verified.\n";
}

void demo_quantity_arithmetic() {
    std::cout << "\n=== Quantity Arithmetic ===\n";

    // Angle conversion.
    Quantity deg180(180.0, "deg");
    auto rad = deg180.convert("rad");
    std::cout << "  Quantity(180, \"deg\") -> convert(\"rad\") = " << rad.value << " rad\n";
    assert(near(rad.value, M_PI));

    // Parsec to meters.
    Quantity pc1(1.0, "pc");
    auto pm = pc1.convert("m");
    std::cout << "  Quantity(1, \"pc\") -> convert(\"m\") = " << std::scientific << pm.value
              << std::fixed << " m\n";
    assert(near(pm.value, 3.0857e16, 1e-3));

    // Jansky.
    Quantity jy1(1.0, "Jy");
    std::cout << "  Quantity(1, \"Jy\"): factor=" << std::scientific << jy1.unit_value().factor()
              << std::fixed << "\n";

    // Addition with conversion.
    Quantity a(5.0, "km");
    Quantity b(3000.0, "m");
    auto sum = a + b;
    std::cout << "  5 km + 3000 m = " << sum.value << " km\n";
    assert(near(sum.value, 8.0));

    // Division for speed.
    Quantity dist(100.0, "km");
    Quantity time(2.0, "h");
    auto speed = dist / time;
    std::cout << "  100 km / 2 h = " << speed.value << " " << speed.get_unit() << "\n";
    assert(near(speed.value, 50.0));

    std::cout << "  [OK] Quantity arithmetic verified.\n";
}

void demo_prefix_system() {
    std::cout << "\n=== Prefix System ===\n";

    Unit km("km");
    Unit mhz("MHz");
    Unit ujy("uJy");

    std::cout << "  1 km = " << km.value().factor() << " m\n";
    assert(near(km.value().factor(), 1000.0));

    std::cout << "  1 MHz = " << std::scientific << mhz.value().factor() << std::fixed << " Hz\n";
    assert(near(mhz.value().factor(), 1e6));

    std::cout << "  1 uJy = " << std::scientific << ujy.value().factor() << std::fixed
              << " W.m-2.Hz-1 equivalent\n";
    assert(near(ujy.value().factor(), 1e-32));

    std::cout << "  [OK] Prefix system verified.\n";
}

void demo_astronomical_units() {
    std::cout << "\n=== Astronomical Units ===\n";

    Quantity au1(1.0, "AU");
    auto au_m = au1.convert("m");
    std::cout << "  1 AU = " << std::scientific << au_m.value << std::fixed << " m\n";
    assert(near(au_m.value, 299792458.0 * 499.0047837));

    Quantity pc1(1.0, "pc");
    auto pc_m = pc1.convert("m");
    std::cout << "  1 pc = " << std::scientific << pc_m.value << std::fixed << " m\n";
    assert(near(pc_m.value, 3.0857e16, 1e-3));

    Quantity ly1(1.0, "ly");
    auto ly_m = ly1.convert("m");
    std::cout << "  1 ly = " << std::scientific << ly_m.value << std::fixed << " m\n";
    assert(near(ly_m.value, 9.46073047e+15));

    Quantity jy1(1.0, "Jy");
    std::cout << "  1 Jy = " << std::scientific << jy1.unit_value().factor() << std::fixed
              << " W/m^2/Hz\n";
    assert(near(jy1.unit_value().factor(), 1e-26));

    std::cout << "  [OK] Astronomical units verified.\n";
}

} // namespace

int main() {
    std::cout << "=== casacore-mini Demo: Unit System ===\n";

    demo_unit_basics();
    demo_quantity_arithmetic();
    demo_prefix_system();
    demo_astronomical_units();

    std::cout << "\n=== All Unit demos passed. ===\n";
    return 0;
}
