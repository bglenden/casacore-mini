#include "casacore_mini/measure_convert.hpp"
#include "casacore_mini/measure_types.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

[[maybe_unused]] constexpr double kDayTol = 1.0e-12;  // ~0.1 microsecond
[[maybe_unused]] constexpr double kRadTol = 1.0e-9;   // ~0.2 mas
[[maybe_unused]] constexpr double kMeterTol = 1.0e-3; // 1 mm

[[maybe_unused]] bool near(double a, double b, double tol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

[[maybe_unused]] bool near_angle(double a, double b, double tol) {
    return std::abs(std::remainder(a - b, 2.0 * M_PI)) < tol;
}

// ---------------------------------------------------------------------------
// Epoch conversions
// ---------------------------------------------------------------------------

bool test_epoch_utc_to_tai() {
    using namespace casacore_mini;
    // MJD 59000.5 UTC
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    auto result = convert_measure(m, EpochRef::tai);
    const auto& ev = std::get<EpochValue>(result.value);
    // TAI-UTC = 37 seconds = 37/86400 days
    [[maybe_unused]] double expected_offset = 37.0 / 86400.0;
    [[maybe_unused]] double result_mjd = ev.day + ev.fraction;
    [[maybe_unused]] double input_mjd = 59000.5;
    assert(near(result_mjd - input_mjd, expected_offset, 1.0e-10));
    return true;
}

bool test_epoch_utc_to_tt() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    auto result = convert_measure(m, EpochRef::tdt);
    const auto& ev = std::get<EpochValue>(result.value);
    // TT-UTC = 37 + 32.184 = 69.184 seconds
    [[maybe_unused]] double expected_offset = 69.184 / 86400.0;
    [[maybe_unused]] double result_mjd = ev.day + ev.fraction;
    [[maybe_unused]] double input_mjd = 59000.5;
    assert(near(result_mjd - input_mjd, expected_offset, 1.0e-8));
    return true;
}

bool test_epoch_round_trip_utc_tai_utc() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    auto tai = convert_measure(m, EpochRef::tai);
    auto back = convert_measure(tai, EpochRef::utc);
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(back.value);
    assert(near(ev.day + ev.fraction, 59000.5, kDayTol));
    return true;
}

bool test_epoch_utc_to_tdb() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    auto tdb = convert_measure(m, EpochRef::tdb);
    // TDB ≈ TT + small periodic term (~1.6ms max).
    // TT-UTC = 69.184s, so TDB-UTC ≈ 69.184s ± 0.002s
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(tdb.value);
    [[maybe_unused]] double diff_s = (ev.day + ev.fraction - 59000.5) * 86400.0;
    assert(std::abs(diff_s - 69.184) < 0.005);

    // Round-trip.
    auto back = convert_measure(tdb, EpochRef::utc);
    [[maybe_unused]] const auto& bv = std::get<EpochValue>(back.value);
    assert(near(bv.day + bv.fraction, 59000.5, 1.0e-10));
    return true;
}

bool test_epoch_utc_to_ut1() {
    using namespace casacore_mini;
    MeasureFrame frame;
    frame.dut1 = -0.17; // typical DUT1 value in seconds

    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    auto ut1 = convert_measure(m, EpochRef::ut1, frame);
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(ut1.value);
    [[maybe_unused]] double diff_s = (ev.day + ev.fraction - 59000.5) * 86400.0;
    assert(near(diff_s, -0.17, 0.001));

    // Round-trip.
    auto back = convert_measure(ut1, EpochRef::utc, frame);
    [[maybe_unused]] const auto& bv = std::get<EpochValue>(back.value);
    assert(near(bv.day + bv.fraction, 59000.5, kDayTol));
    return true;
}

// ---------------------------------------------------------------------------
// Position conversions
// ---------------------------------------------------------------------------

bool test_position_itrf_to_wgs84_round_trip() {
    using namespace casacore_mini;
    // VLA ITRF coordinates.
    PositionValue itrf{-1601185.4, -5041977.5, 3554875.9};
    Measure m{MeasureType::position, MeasureRef{PositionRef::itrf, std::nullopt}, itrf};
    auto wgs84 = convert_measure(m, PositionRef::wgs84);
    auto back = convert_measure(wgs84, PositionRef::itrf);
    [[maybe_unused]] const auto& bv = std::get<PositionValue>(back.value);
    assert(near(bv.x_m, itrf.x_m, kMeterTol));
    assert(near(bv.y_m, itrf.y_m, kMeterTol));
    assert(near(bv.z_m, itrf.z_m, kMeterTol));
    return true;
}

bool test_position_wgs84_values() {
    using namespace casacore_mini;
    // VLA: lon≈-107.618°, lat≈34.079°, h≈2124m
    PositionValue itrf{-1601185.4, -5041977.5, 3554875.9};
    Measure m{MeasureType::position, MeasureRef{PositionRef::itrf, std::nullopt}, itrf};
    auto wgs84 = convert_measure(m, PositionRef::wgs84);
    const auto& pv = std::get<PositionValue>(wgs84.value);
    [[maybe_unused]] double lon_deg = pv.x_m * 180.0 / M_PI;
    [[maybe_unused]] double lat_deg = pv.y_m * 180.0 / M_PI;
    assert(std::abs(lon_deg - (-107.618)) < 0.01);
    assert(std::abs(lat_deg - 34.079) < 0.01);
    assert(std::abs(pv.z_m - 2124.0) < 50.0); // height within 50 meters (approx coords)
    return true;
}

// ---------------------------------------------------------------------------
// Direction conversions
// ---------------------------------------------------------------------------

bool test_direction_j2000_to_galactic() {
    using namespace casacore_mini;
    // RA=180°, Dec=45° in J2000.
    double ra = M_PI;
    double dec = M_PI / 4.0;
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
              DirectionValue{ra, dec}};
    auto gal = convert_measure(m, DirectionRef::galactic);
    // Round-trip.
    auto back = convert_measure(gal, DirectionRef::j2000);
    [[maybe_unused]] const auto& dv = std::get<DirectionValue>(back.value);
    assert(near(dv.lon_rad, ra, kRadTol));
    assert(near(dv.lat_rad, dec, kRadTol));
    return true;
}

bool test_direction_galactic_center() {
    using namespace casacore_mini;
    // Galactic center: l=0, b=0 → approximately RA≈266.405°, Dec≈-28.936°.
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::galactic, std::nullopt},
              DirectionValue{0.0, 0.0}};
    auto j2000 = convert_measure(m, DirectionRef::j2000);
    const auto& dv = std::get<DirectionValue>(j2000.value);
    [[maybe_unused]] double ra_deg = dv.lon_rad * 180.0 / M_PI;
    [[maybe_unused]] double dec_deg = dv.lat_rad * 180.0 / M_PI;
    assert(std::abs(ra_deg - 266.405) < 0.01);
    assert(std::abs(dec_deg - (-28.936)) < 0.01);
    return true;
}

bool test_direction_j2000_to_ecliptic() {
    using namespace casacore_mini;
    double ra = M_PI;
    double dec = M_PI / 6.0; // 30 degrees
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
              DirectionValue{ra, dec}};
    auto ecl = convert_measure(m, DirectionRef::ecliptic);
    auto back = convert_measure(ecl, DirectionRef::j2000);
    [[maybe_unused]] const auto& dv = std::get<DirectionValue>(back.value);
    assert(near(dv.lon_rad, ra, kRadTol));
    assert(near(dv.lat_rad, dec, kRadTol));
    return true;
}

bool test_direction_j2000_to_app() {
    using namespace casacore_mini;
    // J2000 RA=180°, Dec=45° at epoch MJD 59000.5 UTC.
    MeasureFrame frame;
    frame.epoch = Measure{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
                          EpochValue{59000.0, 0.5}};

    double ra = M_PI;
    double dec = M_PI / 4.0;
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
              DirectionValue{ra, dec}};
    auto app = convert_measure(m, DirectionRef::app, frame);

    // APP differs from J2000 due to precession/nutation (~20y offset → several arcmin).
    [[maybe_unused]] const auto& dv = std::get<DirectionValue>(app.value);
    assert(std::abs(dv.lon_rad - ra) < 0.01); // within ~0.6 degrees
    assert(std::abs(dv.lat_rad - dec) < 0.01);

    // Round-trip.
    auto back = convert_measure(app, DirectionRef::j2000, frame);
    [[maybe_unused]] const auto& bv = std::get<DirectionValue>(back.value);
    assert(near(bv.lon_rad, ra, kRadTol));
    assert(near(bv.lat_rad, dec, kRadTol));
    return true;
}

bool test_direction_b1950_to_j2000_roundtrip() {
    using namespace casacore_mini;
    // Pair from casacore-original dM1950_2000.cc:
    // B1950: 13h28m49.657756, 30d45m58.64060
    // J2000: 13h31m08.288048, 30d30m32.95924
    const double b1950_ra_deg = 202.20690731666667;
    const double b1950_dec_deg = 30.766289055555557;
    const double j2000_ra_deg = 202.78453353333336;
    const double j2000_dec_deg = 30.509155344444444;

    const double b1950_ra = b1950_ra_deg * M_PI / 180.0;
    const double b1950_dec = b1950_dec_deg * M_PI / 180.0;
    const double j2000_ra = j2000_ra_deg * M_PI / 180.0;
    const double j2000_dec = j2000_dec_deg * M_PI / 180.0;

    Measure m{MeasureType::direction, MeasureRef{DirectionRef::b1950, std::nullopt},
              DirectionValue{b1950_ra, b1950_dec}};
    auto j2k = convert_measure(m, DirectionRef::j2000);
    const auto& j2k_val = std::get<DirectionValue>(j2k.value);
    assert(near_angle(j2k_val.lon_rad, j2000_ra, 5.0e-6));
    assert(near(j2k_val.lat_rad, j2000_dec, 5.0e-6));

    auto back = convert_measure(j2k, DirectionRef::b1950);
    const auto& back_val = std::get<DirectionValue>(back.value);
    assert(near_angle(back_val.lon_rad, b1950_ra, 5.0e-6));
    assert(near(back_val.lat_rad, b1950_dec, 5.0e-6));
    return true;
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

bool test_type_mismatch_throws() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    try {
        (void)convert_measure(m, DirectionRef::j2000); // wrong type
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_unsupported_epoch_ref_throws() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    try {
        (void)convert_measure(m, EpochRef::gast); // not yet supported
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_app_without_frame_throws() {
    using namespace casacore_mini;
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
              DirectionValue{M_PI, 0.5}};
    try {
        (void)convert_measure(m, DirectionRef::app); // no epoch in frame
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

    std::cout << "measure_convert_test\n";
    run("epoch_utc_to_tai", test_epoch_utc_to_tai);
    run("epoch_utc_to_tt", test_epoch_utc_to_tt);
    run("epoch_round_trip_utc_tai_utc", test_epoch_round_trip_utc_tai_utc);
    run("epoch_utc_to_tdb", test_epoch_utc_to_tdb);
    run("epoch_utc_to_ut1", test_epoch_utc_to_ut1);
    run("position_itrf_to_wgs84_round_trip", test_position_itrf_to_wgs84_round_trip);
    run("position_wgs84_values", test_position_wgs84_values);
    run("direction_j2000_to_galactic", test_direction_j2000_to_galactic);
    run("direction_galactic_center", test_direction_galactic_center);
    run("direction_j2000_to_ecliptic", test_direction_j2000_to_ecliptic);
    run("direction_j2000_to_app", test_direction_j2000_to_app);
    run("direction_b1950_to_j2000_roundtrip", test_direction_b1950_to_j2000_roundtrip);
    run("type_mismatch_throws", test_type_mismatch_throws);
    run("unsupported_epoch_ref_throws", test_unsupported_epoch_ref_throws);
    run("app_without_frame_throws", test_app_without_frame_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
