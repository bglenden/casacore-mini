// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/erfa_bridge.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

constexpr double kTol = 1.0e-9;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

// MJD 59000.5 = 2020-05-31 12:00:00 UTC (approximately).
constexpr double kTestMjd0 = 2400000.5;
constexpr double kTestMjd = 59000.5;

bool test_utc_tai_round_trip() {
    using namespace casacore_mini;
    double tai1 = 0.0;
    double tai2 = 0.0;
    utc_to_tai(kTestMjd0, kTestMjd, tai1, tai2);

    // TAI-UTC = 37 leap seconds as of 2020.
    // In days: 37/86400 ≈ 0.000428...
    [[maybe_unused]] double tai_mjd = (tai1 - kTestMjd0) + tai2;
    [[maybe_unused]] double diff_seconds = (tai_mjd - kTestMjd) * 86400.0;
    assert(near(diff_seconds, 37.0, 0.1)); // 37 leap seconds

    // Round-trip back to UTC.
    double utc1 = 0.0;
    double utc2 = 0.0;
    tai_to_utc(tai1, tai2, utc1, utc2);
    [[maybe_unused]] double utc_mjd = (utc1 - kTestMjd0) + utc2;
    assert(near(utc_mjd, kTestMjd, 1.0e-12));
    return true;
}

bool test_tai_tt_round_trip() {
    using namespace casacore_mini;
    double tt1 = 0.0;
    double tt2 = 0.0;
    tai_to_tt(kTestMjd0, kTestMjd, tt1, tt2);

    // TT-TAI = 32.184 seconds always.
    [[maybe_unused]] double tt_mjd = (tt1 - kTestMjd0) + tt2;
    [[maybe_unused]] double diff_seconds = (tt_mjd - kTestMjd) * 86400.0;
    assert(near(diff_seconds, 32.184, 0.001));

    double tai1 = 0.0;
    double tai2 = 0.0;
    tt_to_tai(tt1, tt2, tai1, tai2);
    [[maybe_unused]] double tai_mjd = (tai1 - kTestMjd0) + tai2;
    assert(near(tai_mjd, kTestMjd, 1.0e-12));
    return true;
}

bool test_wgs84_itrf_round_trip() {
    using namespace casacore_mini;
    // VLA location (approximate): lon=-107.618°, lat=34.079°, h=2124m.
    double lon = -107.618 * M_PI / 180.0;
    double lat = 34.079 * M_PI / 180.0;
    double height = 2124.0;

    double xyz[3]; // NOLINT
    wgs84_to_itrf(lon, lat, height, xyz);

    // VLA ITRF coordinates are approximately:
    // X ≈ -1601185 m, Y ≈ -5041977 m, Z ≈ 3554876 m
    assert(std::abs(xyz[0] - (-1601185.0)) < 200.0);
    assert(std::abs(xyz[1] - (-5041977.0)) < 200.0);
    assert(std::abs(xyz[2] - 3554876.0) < 200.0);

    // Round-trip.
    double lon2 = 0.0;
    double lat2 = 0.0;
    double h2 = 0.0;
    itrf_to_wgs84(xyz, lon2, lat2, h2);
    assert(near(lon2, lon, 1.0e-10));
    assert(near(lat2, lat, 1.0e-10));
    assert(near(h2, height, 1.0e-3)); // 1 mm tolerance
    return true;
}

bool test_j2000_galactic_round_trip() {
    using namespace casacore_mini;
    // Galactic center in J2000: RA ≈ 266.405°, Dec ≈ -28.936°.
    double ra = 266.405 * M_PI / 180.0;
    double dec = -28.936 * M_PI / 180.0;

    double l = 0.0;
    double b = 0.0;
    j2000_to_galactic(ra, dec, l, b);
    // Should be near l=0, b=0 (galactic center).
    assert(near(l, 0.0, 0.01));
    assert(near(b, 0.0, 0.01));

    // Round-trip.
    double ra2 = 0.0;
    double dec2 = 0.0;
    galactic_to_j2000(l, b, ra2, dec2);
    assert(near(ra2, ra, 1.0e-6));
    assert(near(dec2, dec, 1.0e-6));
    return true;
}

bool test_j2000_ecliptic_round_trip() {
    using namespace casacore_mini;
    double ra = M_PI;        // 180 degrees
    double dec = M_PI / 4.0; // 45 degrees

    double elon = 0.0;
    double elat = 0.0;
    j2000_to_ecliptic(ra, dec, elon, elat);

    double ra2 = 0.0;
    double dec2 = 0.0;
    ecliptic_to_j2000(elon, elat, ra2, dec2);

    assert(near(ra2, ra, 1.0e-10));
    assert(near(dec2, dec, 1.0e-10));
    return true;
}

bool test_bias_precession_nutation() {
    using namespace casacore_mini;
    // At J2000.0 itself, the BPN matrix should be ~identity (plus frame bias).
    double tt1 = 2451545.0; // J2000.0 TT
    double tt2 = 0.0;
    std::array<std::array<double, 3>, 3> rbpn{};
    bias_precession_nutation_matrix(tt1, tt2, rbpn);

    // Diagonal should be close to 1.0.
    assert(near(rbpn[0][0], 1.0, 1.0e-5));
    assert(near(rbpn[1][1], 1.0, 1.0e-5));
    assert(near(rbpn[2][2], 1.0, 1.0e-5));

    // Off-diagonal should be small (frame bias is ~few mas).
    assert(std::abs(rbpn[0][1]) < 1.0e-4);
    assert(std::abs(rbpn[0][2]) < 1.0e-4);
    return true;
}

bool test_approx_dtdb() {
    using namespace casacore_mini;
    // TDB-TT is periodic with amplitude ~1.6 ms.
    [[maybe_unused]] double dtr = approx_dtdb(2451545.0, 0.0);
    assert(std::abs(dtr) < 0.002); // within 2 ms
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

    std::cout << "erfa_bridge_test\n";
    run("utc_tai_round_trip", test_utc_tai_round_trip);
    run("tai_tt_round_trip", test_tai_tt_round_trip);
    run("wgs84_itrf_round_trip", test_wgs84_itrf_round_trip);
    run("j2000_galactic_round_trip", test_j2000_galactic_round_trip);
    run("j2000_ecliptic_round_trip", test_j2000_ecliptic_round_trip);
    run("bias_precession_nutation", test_bias_precession_nutation);
    run("approx_dtdb", test_approx_dtdb);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
