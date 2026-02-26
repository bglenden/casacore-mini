#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-8;
constexpr double kDeg2Rad = M_PI / 180.0;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// Build direction + spectral + stokes system
// ---------------------------------------------------------------------------
bool test_composition() {
    CoordinateSystem cs;

    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 180.0 * kDeg2Rad,
        45.0 * kDeg2Rad, -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 128.0,
        128.0);
    cs.add_coordinate(std::move(dc));

    auto sc = std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0);
    cs.add_coordinate(std::move(sc));

    auto stokes = std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2, 3, 4});
    cs.add_coordinate(std::move(stokes));

    assert(cs.n_coordinates() == 3);
    assert(cs.n_pixel_axes() == 4); // 2 (dir) + 1 (spec) + 1 (stokes)
    assert(cs.n_world_axes() == 4);

    // Verify axis mapping.
    [[maybe_unused]] auto dir_world = cs.find_world_axis(0);
    assert(dir_world.has_value());
    assert(dir_world->first == 0); // direction coordinate

    [[maybe_unused]] auto spec_world = cs.find_world_axis(2);
    assert(spec_world.has_value());
    assert(spec_world->first == 1); // spectral coordinate

    [[maybe_unused]] auto stokes_world = cs.find_world_axis(3);
    assert(stokes_world.has_value());
    assert(stokes_world->first == 2); // stokes coordinate

    return true;
}

// ---------------------------------------------------------------------------
// ObsInfo integration
// ---------------------------------------------------------------------------
bool test_obs_info() {
    CoordinateSystem cs;
    ObsInfo info;
    info.telescope = "VLA";
    info.observer = "TestUser";
    cs.set_obs_info(std::move(info));

    assert(cs.obs_info().telescope == "VLA");
    assert(cs.obs_info().observer == "TestUser");
    return true;
}

// ---------------------------------------------------------------------------
// Record round-trip
// ---------------------------------------------------------------------------
bool test_record_roundtrip() {
    CoordinateSystem cs;

    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0, 0.0,
        -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 50.0, 50.0);
    cs.add_coordinate(std::move(dc));

    auto sc = std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0);
    cs.add_coordinate(std::move(sc));

    auto stokes = std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2, 3, 4});
    cs.add_coordinate(std::move(stokes));

    ObsInfo info;
    info.telescope = "ALMA";
    info.observer = "Astronomer";
    info.obs_date = Measure{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
                            EpochValue{59000.0, 0.5}};
    cs.set_obs_info(std::move(info));

    Record rec = cs.save();
    CoordinateSystem restored = CoordinateSystem::restore(rec);

    assert(restored.n_coordinates() == 3);
    assert(restored.n_pixel_axes() == 4);
    assert(restored.n_world_axes() == 4);
    assert(restored.obs_info().telescope == "ALMA");
    assert(restored.obs_info().observer == "Astronomer");
    assert(restored.obs_info().obs_date.has_value());

    // Verify direction coordinate transform parity.
    auto w1 = cs.coordinate(0).to_world({55.0, 55.0});
    auto w2 = restored.coordinate(0).to_world({55.0, 55.0});
    assert(near(w1[0], w2[0]));
    assert(near(w1[1], w2[1]));

    // Verify spectral parity.
    auto sw1 = cs.coordinate(1).to_world({5.0});
    auto sw2 = restored.coordinate(1).to_world({5.0});
    assert(near(sw1[0], sw2[0]));

    // Verify Stokes parity.
    auto st1 = cs.coordinate(2).to_world({2.0});
    auto st2 = restored.coordinate(2).to_world({2.0});
    assert(st1[0] == st2[0]);

    return true;
}

// ---------------------------------------------------------------------------
// Individual coordinate access by type
// ---------------------------------------------------------------------------
bool test_coordinate_types() {
    CoordinateSystem cs;

    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0, 0.0,
        -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 50.0, 50.0));

    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    assert(cs.coordinate(0).type() == CoordinateType::direction);
    assert(cs.coordinate(1).type() == CoordinateType::spectral);
    return true;
}

// ---------------------------------------------------------------------------
// Empty coordinate system
// ---------------------------------------------------------------------------
bool test_empty_system() {
    CoordinateSystem cs;
    assert(cs.n_coordinates() == 0);
    assert(cs.n_pixel_axes() == 0);
    assert(cs.n_world_axes() == 0);

    Record rec = cs.save();
    CoordinateSystem restored = CoordinateSystem::restore(rec);
    assert(restored.n_coordinates() == 0);
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

    std::cout << "coordinate_system_test\n";
    run("composition", test_composition);
    run("obs_info", test_obs_info);
    run("record_roundtrip", test_record_roundtrip);
    run("coordinate_types", test_coordinate_types);
    run("empty_system", test_empty_system);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
