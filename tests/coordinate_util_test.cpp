#include "casacore_mini/coordinate_util.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace casacore_mini;

constexpr double kDeg2Rad = M_PI / 180.0;

bool test_find_direction() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0, 0.0,
        -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 50.0, 50.0));

    auto idx = find_direction_coordinate(cs);
    assert(idx.has_value());
    assert(*idx == 1);
    return true;
}

bool test_find_spectral() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2}));
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    auto idx = find_spectral_coordinate(cs);
    assert(idx.has_value());
    assert(*idx == 1);
    return true;
}

bool test_find_stokes() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));
    cs.add_coordinate(std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2, 3, 4}));

    auto idx = find_stokes_coordinate(cs);
    assert(idx.has_value());
    assert(*idx == 1);
    return true;
}

bool test_find_not_present() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    assert(!find_direction_coordinate(cs).has_value());
    assert(!find_stokes_coordinate(cs).has_value());
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

    std::cout << "coordinate_util_test\n";
    run("find_direction", test_find_direction);
    run("find_spectral", test_find_spectral);
    run("find_stokes", test_find_stokes);
    run("find_not_present", test_find_not_present);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
