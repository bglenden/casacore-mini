// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/linear_coordinate.hpp"
#include "casacore_mini/quality_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"
#include "casacore_mini/tabular_coordinate.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-8;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// All 6 types save/restore via Coordinate::restore factory
// ---------------------------------------------------------------------------
bool test_direction_restore() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           0.0, -M_PI / 180.0 / 3600.0, M_PI / 180.0 / 3600.0, {}, 50.0, 50.0);
    Record rec = dc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::direction);
    auto w = restored->to_world({55.0, 55.0});
    auto w2 = dc.to_world({55.0, 55.0});
    assert(near(w[0], w2[0]));
    assert(near(w[1], w2[1]));
    return true;
}

bool test_spectral_restore() {
    SpectralCoordinate sc(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0, 1.42e9);
    Record rec = sc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::spectral);
    auto w = restored->to_world({5.0});
    assert(near(w[0], 1.42e9 + 5e6));
    return true;
}

bool test_stokes_restore() {
    StokesCoordinate sc({1, 2, 3, 4});
    Record rec = sc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::stokes);
    auto w = restored->to_world({2.0});
    assert(w[0] == 3.0);
    return true;
}

bool test_linear_restore() {
    LinearXform xf;
    xf.crpix = {0.0, 0.0};
    xf.cdelt = {1.0, 2.0};
    xf.crval = {10.0, 20.0};
    LinearCoordinate lc({"X", "Y"}, {"m", "m"}, std::move(xf));
    Record rec = lc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::linear);
    auto w = restored->to_world({5.0, 5.0});
    assert(near(w[0], 15.0));
    assert(near(w[1], 30.0));
    return true;
}

bool test_tabular_restore() {
    TabularCoordinate tc({0.0, 1.0, 2.0, 3.0}, {100.0, 200.0, 400.0, 800.0}, "Power", "W");
    Record rec = tc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::tabular);
    auto w = restored->to_world({0.5});
    assert(near(w[0], 150.0, 1e-6)); // midpoint between 100 and 200
    return true;
}

bool test_quality_restore() {
    QualityCoordinate qc({0, 1});
    Record rec = qc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::quality);
    auto w = restored->to_world({1.0});
    assert(w[0] == 1.0);
    return true;
}

// ---------------------------------------------------------------------------
// Missing coordinate_type throws
// ---------------------------------------------------------------------------
bool test_missing_type_throws() {
    Record rec;
    rec.set("some_field", RecordValue(42.0));
    try {
        (void)Coordinate::restore(rec);
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

    std::cout << "coordinate_record_test\n";
    run("direction_restore", test_direction_restore);
    run("spectral_restore", test_spectral_restore);
    run("stokes_restore", test_stokes_restore);
    run("linear_restore", test_linear_restore);
    run("tabular_restore", test_tabular_restore);
    run("quality_restore", test_quality_restore);
    run("missing_type_throws", test_missing_type_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
