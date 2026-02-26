// demo_coordinate.cpp -- Phase 8: Coordinate system + transforms
//
// casacore-original equivalent (dCoordinates.cc partial):
//   Matrix<Double> xform(2,2); xform = 0.0; xform(0,0)=1; xform(1,1)=1;
//   DirectionCoordinate dc(MDirection::J2000, Projection::SIN,
//       135*C::degree, 60*C::degree, -1*C::degree, 1*C::degree,
//       xform, 128, 128);
//   Vector<Double> pixel(2), world(2);
//   pixel(0) = 138; pixel(1) = 138;
//   dc.toWorld(world, pixel);
//
//   StokesCoordinate stokes(Vector<Int>(4, {Stokes::I, Q, U, V}));
//   SpectralCoordinate spec(MFrequency::TOPO, 1400e6, 20e3, 0.0);
//
//   CoordinateSystem cs;
//   cs.addCoordinate(dc); cs.addCoordinate(stokes); cs.addCoordinate(spec);
//   Record rec;
//   cs.save(rec, "");
//   CoordinateSystem cs2;
//   cs2.restore(rec, "");

#include <casacore_mini/coordinate_system.hpp>
#include <casacore_mini/direction_coordinate.hpp>
#include <casacore_mini/measure_types.hpp>
#include <casacore_mini/projection.hpp>
#include <casacore_mini/quantity.hpp>
#include <casacore_mini/spectral_coordinate.hpp>
#include <casacore_mini/stokes_coordinate.hpp>

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

using namespace casacore_mini;

namespace {

constexpr double kTol = 1.0e-6;

bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

} // namespace

// ---------------------------------------------------------------------------
// DirectionCoordinate: J2000 SIN projection
// ---------------------------------------------------------------------------
static void demo_direction_coordinate() {
    std::cout << "\n=== DirectionCoordinate ===\n";

    // J2000, SIN projection, ref=(135,60) deg, 1 deg/px, crpix=(128,128)
    Quantity ref_ra(135.0, "deg");
    Quantity ref_dec(60.0, "deg");
    Quantity inc_ra(-1.0, "deg");
    Quantity inc_dec(1.0, "deg");

    DirectionCoordinate dc(DirectionRef::j2000,
                           Projection{ProjectionType::sin, {0.0, 0.0}},
                           ref_ra.get_value("rad"),     // ref RA
                           ref_dec.get_value("rad"),    // ref Dec
                           inc_ra.get_value("rad"),     // RA increment (-1 deg/px)
                           inc_dec.get_value("rad"),    // Dec increment (1 deg/px)
                           std::vector<double>{},       // identity PC matrix
                           128.0, 128.0);               // crpix

    // Reference pixel -> reference world.
    auto w_ref = dc.to_world({128.0, 128.0});
    std::cout << "  pixel(128,128) -> world: RA="
              << Quantity(w_ref[0], "rad").get_value("deg")
              << " deg, Dec="
              << Quantity(w_ref[1], "rad").get_value("deg") << " deg\n";
    assert(near(w_ref[0], ref_ra.get_value("rad"), 1e-4));
    assert(near(w_ref[1], ref_dec.get_value("rad"), 1e-4));

    // Offset pixel -> world -> round-trip.
    auto w138 = dc.to_world({138.0, 138.0});
    std::cout << "  pixel(138,138) -> world: RA="
              << Quantity(w138[0], "rad").get_value("deg")
              << " deg, Dec="
              << Quantity(w138[1], "rad").get_value("deg") << " deg\n";

    auto p_back = dc.to_pixel(w138);
    std::cout << "  world -> pixel round-trip: (" << p_back[0] << ", " << p_back[1] << ")\n";
    assert(near(p_back[0], 138.0, 1e-3));
    assert(near(p_back[1], 138.0, 1e-3));

    std::cout << "  [OK] DirectionCoordinate verified.\n";
}

// ---------------------------------------------------------------------------
// StokesCoordinate: I, Q, U, V
// ---------------------------------------------------------------------------
static void demo_stokes_coordinate() {
    std::cout << "\n=== StokesCoordinate ===\n";

    StokesCoordinate sc({1, 2, 3, 4}); // I=1, Q=2, U=3, V=4

    auto w0 = sc.to_world({0.0});
    auto w1 = sc.to_world({1.0});
    auto w2 = sc.to_world({2.0});
    auto w3 = sc.to_world({3.0});

    std::cout << "  pixel 0 -> Stokes " << w0[0] << " (I)\n";
    std::cout << "  pixel 1 -> Stokes " << w1[0] << " (Q)\n";
    std::cout << "  pixel 2 -> Stokes " << w2[0] << " (U)\n";
    std::cout << "  pixel 3 -> Stokes " << w3[0] << " (V)\n";

    assert(w0[0] == 1.0);
    assert(w1[0] == 2.0);
    assert(w2[0] == 3.0);
    assert(w3[0] == 4.0);

    // Reverse: Stokes Q -> pixel 1
    auto p_q = sc.to_pixel({2.0});
    std::cout << "  Stokes Q -> pixel " << p_q[0] << "\n";
    assert(std::abs(p_q[0] - 1.0) < 0.01);

    std::cout << "  [OK] StokesCoordinate verified.\n";
}

// ---------------------------------------------------------------------------
// SpectralCoordinate: TOPO, 1400 MHz ref, 20 kHz channels
// ---------------------------------------------------------------------------
static void demo_spectral_coordinate() {
    std::cout << "\n=== SpectralCoordinate ===\n";

    SpectralCoordinate sc(FrequencyRef::topo,
                          1400.0e6,   // ref frequency = 1400 MHz
                          20.0e3,     // cdelt = 20 kHz channel width
                          0.0);       // crpix = channel 0

    // Channel 0 -> 1400 MHz
    auto w0 = sc.to_world({0.0});
    std::cout << "  channel 0 -> " << w0[0] / 1e6 << " MHz\n";
    assert(near(w0[0], 1400.0e6));

    // Channel 50 -> 1400 MHz + 50*20 kHz = 1401 MHz
    auto w50 = sc.to_world({50.0});
    std::cout << "  channel 50 -> " << w50[0] / 1e6 << " MHz\n";
    assert(near(w50[0], 1401.0e6, 1e-6));

    // Round-trip.
    auto p_back = sc.to_pixel(w50);
    std::cout << "  1401 MHz -> channel " << p_back[0] << "\n";
    assert(near(p_back[0], 50.0, 1e-8));

    std::cout << "  [OK] SpectralCoordinate verified.\n";
}

// ---------------------------------------------------------------------------
// CoordinateSystem composition + save/restore
// ---------------------------------------------------------------------------
static void demo_coordinate_system() {
    std::cout << "\n=== CoordinateSystem ===\n";

    CoordinateSystem cs;

    // Add direction coordinate.
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
        Quantity(135.0, "deg").get_value("rad"),
        Quantity(60.0, "deg").get_value("rad"),
        Quantity(-1.0, "deg").get_value("rad"),
        Quantity(1.0, "deg").get_value("rad"),
        std::vector<double>{}, 128.0, 128.0));

    // Add spectral coordinate.
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(
        FrequencyRef::topo, 1400.0e6, 20.0e3, 0.0));

    // Add stokes coordinate.
    cs.add_coordinate(std::make_unique<StokesCoordinate>(
        std::vector<std::int32_t>{1, 2, 3, 4}));

    std::cout << "  Coordinates: " << cs.n_coordinates() << "\n";
    std::cout << "  Pixel axes:  " << cs.n_pixel_axes()
              << " (2 dir + 1 spec + 1 stokes)\n";
    std::cout << "  World axes:  " << cs.n_world_axes() << "\n";
    assert(cs.n_coordinates() == 3);
    assert(cs.n_pixel_axes() == 4);
    assert(cs.n_world_axes() == 4);

    // Axis mapping.
    auto dir_axis = cs.find_world_axis(0);
    assert(dir_axis.has_value());
    std::cout << "  World axis 0 -> coordinate " << dir_axis->first
              << " (direction)\n";
    assert(dir_axis->first == 0);

    auto spec_axis = cs.find_world_axis(2);
    assert(spec_axis.has_value());
    std::cout << "  World axis 2 -> coordinate " << spec_axis->first
              << " (spectral)\n";
    assert(spec_axis->first == 1);

    // Per-coordinate transforms work.
    auto w_dir = cs.coordinate(0).to_world({128.0, 128.0});
    std::cout << "  Direction at crpix: RA="
              << Quantity(w_dir[0], "rad").get_value("deg")
              << " deg, Dec="
              << Quantity(w_dir[1], "rad").get_value("deg") << " deg\n";
    assert(near(w_dir[0], Quantity(135.0, "deg").get_value("rad"), 1e-4));

    auto w_spec = cs.coordinate(1).to_world({50.0});
    std::cout << "  Spectral at chan 50: " << w_spec[0] / 1e6 << " MHz\n";
    assert(near(w_spec[0], 1401.0e6, 1e-6));

    // Save/restore round-trip.
    std::cout << "\n  --- Save/Restore Round-Trip ---\n";
    Record rec = cs.save();
    std::cout << "  Saved to Record with " << rec.size() << " fields\n";

    CoordinateSystem restored = CoordinateSystem::restore(rec);
    assert(restored.n_coordinates() == 3);
    assert(restored.n_pixel_axes() == 4);

    // Verify restored coordinate transforms match original.
    auto w_dir2 = restored.coordinate(0).to_world({128.0, 128.0});
    assert(near(w_dir2[0], w_dir[0]));
    assert(near(w_dir2[1], w_dir[1]));

    auto w_spec2 = restored.coordinate(1).to_world({50.0});
    assert(near(w_spec2[0], w_spec[0]));

    auto w_stokes2 = restored.coordinate(2).to_world({2.0});
    assert(w_stokes2[0] == 3.0); // U

    std::cout << "  [OK] Save/restore verified.\n";

    // Document gap: no composite toWorld()/toPixel() on CoordinateSystem.
    std::cout << "\n  NOTE: casacore-original supports CoordinateSystem.toWorld()\n"
              << "        for composite pixel->world across all axes.\n"
              << "        casacore-mini currently only supports per-coordinate\n"
              << "        transforms via cs.coordinate(i).to_world().\n";
}

int main() {
    std::cout << "=== casacore-mini Demo: Coordinates (Phase 8) ===\n";

    demo_direction_coordinate();
    demo_stokes_coordinate();
    demo_spectral_coordinate();
    demo_coordinate_system();

    std::cout << "\n=== All Coordinate demos passed. ===\n";
    return 0;
}
