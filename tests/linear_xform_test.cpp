// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/linear_xform.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-10;

[[maybe_unused]] bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// 2x2 identity transform
// ---------------------------------------------------------------------------
bool test_2x2_identity() {
    LinearXform xf;
    xf.crpix = {0.0, 0.0};
    xf.cdelt = {1.0, 1.0};
    xf.crval = {0.0, 0.0};
    // empty pc = identity

    auto world = xf.pixel_to_world({5.0, 10.0});
    assert(near(world[0], 5.0));
    assert(near(world[1], 10.0));

    auto pixel = xf.world_to_pixel({5.0, 10.0});
    assert(near(pixel[0], 5.0));
    assert(near(pixel[1], 10.0));
    return true;
}

// ---------------------------------------------------------------------------
// 2x2 with crpix offset and cdelt scaling
// ---------------------------------------------------------------------------
bool test_2x2_scaled() {
    LinearXform xf;
    xf.crpix = {100.0, 200.0};
    xf.cdelt = {0.01, -0.01};
    xf.crval = {3.14, 0.5};
    // identity pc

    auto world = xf.pixel_to_world({100.0, 200.0});
    assert(near(world[0], 3.14));
    assert(near(world[1], 0.5));

    auto world2 = xf.pixel_to_world({110.0, 200.0});
    assert(near(world2[0], 3.14 + 10.0 * 0.01));
    assert(near(world2[1], 0.5));

    // Round-trip.
    auto pixel = xf.world_to_pixel(world2);
    assert(near(pixel[0], 110.0));
    assert(near(pixel[1], 200.0));
    return true;
}

// ---------------------------------------------------------------------------
// 2x2 with rotation (45 degrees)
// ---------------------------------------------------------------------------
bool test_2x2_rotated() {
    double c = std::cos(M_PI / 4.0);
    double s = std::sin(M_PI / 4.0);

    LinearXform xf;
    xf.crpix = {0.0, 0.0};
    xf.cdelt = {1.0, 1.0};
    xf.crval = {0.0, 0.0};
    xf.pc = {c, -s, s, c}; // row-major 2x2 rotation

    auto world = xf.pixel_to_world({1.0, 0.0});
    assert(near(world[0], c));
    assert(near(world[1], s));

    // Round-trip.
    auto pixel = xf.world_to_pixel(world);
    assert(near(pixel[0], 1.0));
    assert(near(pixel[1], 0.0));
    return true;
}

// ---------------------------------------------------------------------------
// 3x3 transform
// ---------------------------------------------------------------------------
bool test_3x3() {
    LinearXform xf;
    xf.crpix = {10.0, 20.0, 30.0};
    xf.cdelt = {2.0, 3.0, 4.0};
    xf.crval = {100.0, 200.0, 300.0};
    // identity pc

    auto world = xf.pixel_to_world({10.0, 20.0, 30.0});
    assert(near(world[0], 100.0));
    assert(near(world[1], 200.0));
    assert(near(world[2], 300.0));

    auto world2 = xf.pixel_to_world({11.0, 22.0, 33.0});
    assert(near(world2[0], 102.0)); // 100 + 2*(11-10)
    assert(near(world2[1], 206.0)); // 200 + 3*(22-20)
    assert(near(world2[2], 312.0)); // 300 + 4*(33-30)

    auto pixel = xf.world_to_pixel(world2);
    assert(near(pixel[0], 11.0));
    assert(near(pixel[1], 22.0));
    assert(near(pixel[2], 33.0));
    return true;
}

// ---------------------------------------------------------------------------
// 1x1 degenerate case
// ---------------------------------------------------------------------------
bool test_1x1() {
    LinearXform xf;
    xf.crpix = {5.0};
    xf.cdelt = {0.001};
    xf.crval = {1.5};

    auto world = xf.pixel_to_world({15.0});
    assert(near(world[0], 1.5 + 0.001 * 10.0)); // 1.51

    auto pixel = xf.world_to_pixel({1.51});
    assert(near(pixel[0], 15.0));
    return true;
}

// ---------------------------------------------------------------------------
// Size mismatch throws
// ---------------------------------------------------------------------------
bool test_size_mismatch_throws() {
    LinearXform xf;
    xf.crpix = {0.0, 0.0};
    xf.cdelt = {1.0, 1.0};
    xf.crval = {0.0, 0.0};

    try {
        (void)xf.pixel_to_world({1.0}); // wrong size
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }

    try {
        (void)xf.world_to_pixel({1.0, 2.0, 3.0}); // wrong size
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
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

    std::cout << "linear_xform_test\n";
    run("2x2_identity", test_2x2_identity);
    run("2x2_scaled", test_2x2_scaled);
    run("2x2_rotated", test_2x2_rotated);
    run("3x3", test_3x3);
    run("1x1", test_1x1);
    run("size_mismatch_throws", test_size_mismatch_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
