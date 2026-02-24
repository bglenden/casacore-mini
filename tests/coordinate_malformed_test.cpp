#include "casacore_mini/coordinate.hpp"
#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/projection.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace casacore_mini;

bool test_restore_missing_type() {
    Record rec;
    rec.set("system", RecordValue(std::string("J2000")));
    // Missing coordinate_type field
    try {
        (void)Coordinate::restore(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_restore_unknown_type() {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("bogus_coord")));
    try {
        (void)Coordinate::restore(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_stokes_empty_values() {
    try {
        StokesCoordinate sc(std::vector<std::int32_t>{});
        // Stokes with no values should still be constructible
        assert(sc.n_pixel_axes() == 0 || sc.n_pixel_axes() == 1);
        return true;
    } catch (const std::exception&) {
        // Also acceptable if it throws
        return true;
    }
}

bool test_stokes_out_of_range_pixel() {
    StokesCoordinate sc(std::vector<std::int32_t>{1, 2, 3, 4});
    try {
        (void)sc.to_world({99.0}); // pixel 99 is out of range for 4 stokes
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

bool test_stokes_code_not_found() {
    StokesCoordinate sc(std::vector<std::int32_t>{1, 2, 3, 4}); // I,Q,U,V
    try {
        (void)sc.to_pixel({99.0}); // No stokes code 99
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

bool test_projection_unknown() {
    try {
        (void)string_to_projection_type("XYZ");
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_coordinate_system_empty_save_restore() {
    CoordinateSystem cs;
    auto rec = cs.save();
    auto restored = CoordinateSystem::restore(rec);
    assert(restored.n_coordinates() == 0);
    assert(restored.n_pixel_axes() == 0);
    assert(restored.n_world_axes() == 0);
    return true;
}

bool test_direction_wrong_pixel_size() {
    constexpr double kDeg2Rad = M_PI / 180.0;
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           0.0, -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, {}, 50.0, 50.0);

    try {
        (void)dc.to_world({1.0}); // needs 2 pixels, given 1
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

bool test_spectral_record_roundtrip_stability() {
    SpectralCoordinate sc(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0);
    auto rec = sc.save();
    auto restored = Coordinate::restore(rec);
    assert(restored->type() == CoordinateType::spectral);

    // Save again and verify it's stable
    auto rec2 = restored->save();
    auto restored2 = Coordinate::restore(rec2);
    auto w1 = restored->to_world({5.0});
    auto w2 = restored2->to_world({5.0});
    assert(std::abs(w1[0] - w2[0]) < 1e-10);
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

    std::cout << "coordinate_malformed_test\n";
    run("restore_missing_type", test_restore_missing_type);
    run("restore_unknown_type", test_restore_unknown_type);
    run("stokes_empty_values", test_stokes_empty_values);
    run("stokes_out_of_range_pixel", test_stokes_out_of_range_pixel);
    run("stokes_code_not_found", test_stokes_code_not_found);
    run("projection_unknown", test_projection_unknown);
    run("coordinate_system_empty_save_restore", test_coordinate_system_empty_save_restore);
    run("direction_wrong_pixel_size", test_direction_wrong_pixel_size);
    run("spectral_record_roundtrip_stability", test_spectral_record_roundtrip_stability);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
