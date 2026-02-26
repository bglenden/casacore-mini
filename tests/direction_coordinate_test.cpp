#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/projection.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-8;
constexpr double kDeg2Rad = M_PI / 180.0;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// SIN projection: reference pixel round-trip
// ---------------------------------------------------------------------------
bool test_sin_reference_pixel() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
                           180.0 * kDeg2Rad,            // RA = 180 deg
                           45.0 * kDeg2Rad,             // Dec = 45 deg
                           -1.0 * kDeg2Rad / 3600.0,    // -1 arcsec/pixel
                           1.0 * kDeg2Rad / 3600.0, {}, // identity PC
                           128.0, 128.0);

    // Reference pixel should map to reference world.
    auto world = dc.to_world({128.0, 128.0});
    assert(near(world[0], 180.0 * kDeg2Rad, 1e-6));
    assert(near(world[1], 45.0 * kDeg2Rad, 1e-6));

    // Round-trip.
    auto pixel = dc.to_pixel(world);
    assert(near(pixel[0], 128.0, 1e-6));
    assert(near(pixel[1], 128.0, 1e-6));
    return true;
}

// ---------------------------------------------------------------------------
// TAN projection round-trip
// ---------------------------------------------------------------------------
bool test_tan_roundtrip() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::tan, {}},
                           0.0, // RA = 0
                           0.0, // Dec = 0
                           -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, {}, 50.0, 50.0);

    // Small offset from reference.
    auto world = dc.to_world({55.0, 55.0});
    auto pixel = dc.to_pixel(world);
    assert(near(pixel[0], 55.0, 1e-6));
    assert(near(pixel[1], 55.0, 1e-6));
    return true;
}

// ---------------------------------------------------------------------------
// ARC projection round-trip
// ---------------------------------------------------------------------------
bool test_arc_roundtrip() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::arc, {}},
                           90.0 * kDeg2Rad, // RA = 90 deg
                           30.0 * kDeg2Rad, // Dec = 30 deg
                           -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, {}, 100.0, 100.0);

    auto world = dc.to_world({110.0, 105.0});
    auto pixel = dc.to_pixel(world);
    assert(near(pixel[0], 110.0, 1e-4));
    assert(near(pixel[1], 105.0, 1e-4));
    return true;
}

// ---------------------------------------------------------------------------
// Record round-trip
// ---------------------------------------------------------------------------
bool test_record_roundtrip() {
    DirectionCoordinate dc(DirectionRef::galactic, Projection{ProjectionType::sin, {0.0, 0.0}},
                           120.0 * kDeg2Rad, -5.0 * kDeg2Rad, -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0,
                           {1.0, 0.0, 0.0, 1.0}, 64.0, 64.0, 180.0, 0.0);

    Record rec = dc.save();
    auto restored = DirectionCoordinate::from_record(rec);

    assert(restored->ref_frame() == DirectionRef::galactic);
    assert(restored->projection().type == ProjectionType::sin);

    // Transform parity.
    auto w1 = dc.to_world({70.0, 70.0});
    auto w2 = restored->to_world({70.0, 70.0});
    assert(near(w1[0], w2[0], 1e-10));
    assert(near(w1[1], w2[1], 1e-10));
    return true;
}

// ---------------------------------------------------------------------------
// Clone
// ---------------------------------------------------------------------------
bool test_clone() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           0.0, -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, {}, 50.0, 50.0);

    auto cloned = dc.clone();
    assert(cloned->type() == CoordinateType::direction);
    auto w1 = dc.to_world({55.0, 55.0});
    auto w2 = cloned->to_world({55.0, 55.0});
    assert(near(w1[0], w2[0], 1e-10));
    assert(near(w1[1], w2[1], 1e-10));
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

    std::cout << "direction_coordinate_test\n";
    run("sin_reference_pixel", test_sin_reference_pixel);
    run("tan_roundtrip", test_tan_roundtrip);
    run("arc_roundtrip", test_arc_roundtrip);
    run("record_roundtrip", test_record_roundtrip);
    run("clone", test_clone);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
