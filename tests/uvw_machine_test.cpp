// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/uvw_machine.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

constexpr double kPi = M_PI;
constexpr double kTol = 1.0e-10;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

bool test_identity_rotation() {
    using namespace casacore_mini;
    // Same phase center → identity matrix.
    double ra = 1.0;
    double dec = 0.5;
    [[maybe_unused]] auto mat = uvw_rotation_matrix(ra, dec, ra, dec);
    // Diagonal should be 1, off-diagonal 0.
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            [[maybe_unused]] double expected = (i == j) ? 1.0 : 0.0;
            assert(near(mat[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)], expected,
                        1e-12));
        }
    }
    return true;
}

bool test_roundtrip_rotation() {
    using namespace casacore_mini;
    double ra1 = 0.3;
    double dec1 = 0.7;
    double ra2 = 1.5;
    double dec2 = -0.2;

    auto fwd = uvw_rotation_matrix(ra1, dec1, ra2, dec2);
    auto rev = uvw_rotation_matrix(ra2, dec2, ra1, dec1);

    UvwValue orig{100.0, 200.0, 300.0};
    auto rotated = rotate_uvw(fwd, orig);
    [[maybe_unused]] auto back = rotate_uvw(rev, rotated);

    assert(near(back.u_m, orig.u_m));
    assert(near(back.v_m, orig.v_m));
    assert(near(back.w_m, orig.w_m));
    return true;
}

bool test_known_vector() {
    using namespace casacore_mini;
    // Rotate UVW = (1, 0, 0) from ra=0, dec=0 to ra=pi/2, dec=0.
    auto mat = uvw_rotation_matrix(0.0, 0.0, kPi / 2.0, 0.0);
    UvwValue v{1.0, 0.0, 0.0};
    auto result = rotate_uvw(mat, v);
    // The rotation should produce a non-trivial result.
    // Check that the length is preserved.
    [[maybe_unused]] double len =
        std::sqrt(result.u_m * result.u_m + result.v_m * result.v_m + result.w_m * result.w_m);
    assert(near(len, 1.0));
    return true;
}

bool test_uvw_machine_wrapper() {
    using namespace casacore_mini;
    UvwMachine machine(0.3, -0.2, 0.8, 0.1);
    UvwValue v{10.0, -5.0, 2.0};
    [[maybe_unused]] auto via_class = machine.convert(v);
    [[maybe_unused]] auto via_free = rotate_uvw(machine.rotation_matrix(), v);
    assert(near(via_class.u_m, via_free.u_m));
    assert(near(via_class.v_m, via_free.v_m));
    assert(near(via_class.w_m, via_free.w_m));
    return true;
}

bool test_parallactic_angle_at_transit() {
    using namespace casacore_mini;
    // At transit (ha=0), parallactic angle is always 0.
    [[maybe_unused]] double pa = parallactic_angle(0.0, 0.5, 0.8);
    assert(near(pa, 0.0));
    return true;
}

bool test_parallactic_angle_known() {
    using namespace casacore_mini;
    // Known ERFA result: eraHd2pa(1.0, 0.5, 0.8) ≈ 0.9171400942422210
    [[maybe_unused]] double pa = parallactic_angle(1.0, 0.5, 0.8);
    assert(near(pa, 0.9171400942422210, 1e-12));
    return true;
}

bool test_parangle_machine_wrapper() {
    using namespace casacore_mini;
    ParAngleMachine machine(0.8);
    [[maybe_unused]] double pa = machine.compute(1.0, 0.5);
    assert(near(pa, parallactic_angle(1.0, 0.5, 0.8)));
    return true;
}

bool test_earth_magnetic_throws() {
    using namespace casacore_mini;
    try {
        earth_magnetic_field(0.0, 0.0, 0.0, 2020.0);
        assert(false && "Should have thrown");
    } catch (const std::runtime_error& e) {
        // Expected: "IGRF not implemented".
        (void)e;
    }
    return true;
}

bool test_earth_magnetic_machine_throws() {
    using namespace casacore_mini;
    EarthMagneticMachine machine(2020.0);
    try {
        machine.compute(0.0, 0.0, 0.0);
        assert(false && "Should have thrown");
    } catch (const std::runtime_error& e) {
        (void)e;
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

    std::cout << "uvw_machine_test\n";
    run("identity_rotation", test_identity_rotation);
    run("roundtrip_rotation", test_roundtrip_rotation);
    run("known_vector", test_known_vector);
    run("uvw_machine_wrapper", test_uvw_machine_wrapper);
    run("parallactic_angle_at_transit", test_parallactic_angle_at_transit);
    run("parallactic_angle_known", test_parallactic_angle_known);
    run("parangle_machine_wrapper", test_parangle_machine_wrapper);
    run("earth_magnetic_throws", test_earth_magnetic_throws);
    run("earth_magnetic_machine_throws", test_earth_magnetic_machine_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
