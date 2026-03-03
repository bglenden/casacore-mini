// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// demo_measure_m1950_2000.cpp -- Phase 8: B1950<->J2000 transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/measures/Measures/test/dM1950_2000.cc
//     Quantity::read(ra, "13h28m49.657756");
//     Quantity::read(dec,"30d45m58.64060");
//     MVDirection b1950(ra, dec);
//     Quantity::read(ra, "13h31m08.288048");
//     Quantity::read(dec,"30d30m32.95924");
//     MVDirection j2000(ra, dec);
//     conB1950(b1950, j2000);
//
// This casacore-mini demo transliterates the same B1950/J2000 checks
// using DirectionRef::b1950 and DirectionRef::j2000 conversions.

#include <casacore_mini/measure_convert.hpp>
#include <casacore_mini/measure_types.hpp>

#include <array>
#include "demo_check.hpp"
#include <cmath>
#include <iostream>

using namespace casacore_mini;

namespace {

struct B1950J2000Sample {
    double b1950_ra_deg;
    double b1950_dec_deg;
    double j2000_ra_deg;
    double j2000_dec_deg;
};

constexpr std::array<B1950J2000Sample, 4> kSamples{{
    {202.20690731666667, 30.766289055555557, 202.78453353333336, 30.509155344444444},
    {49.1231970375, 41.33108799166667, 49.9506671625, 41.511695525},
    {202.20690708333333, 30.766288888888887, 202.7845332666667, 30.509155236111113},
    {202.20691250000002, 30.766294444444444, 202.78453333333337, 30.50915527777778},
}};

double deg_to_rad(double deg) {
    return deg * M_PI / 180.0;
}

double rad_to_arcsec(double rad) {
    return rad * 180.0 / M_PI * 3600.0;
}

double angle_diff_rad(double a, double b) {
    return std::remainder(a - b, 2.0 * M_PI);
}

} // namespace

int main() {
    try {
        std::cout << "=== casacore-mini Demo: B1950 <-> J2000 (Phase 8) ===\n";

        for (std::size_t i = 0; i < kSamples.size(); ++i) {
            const auto& s = kSamples[i];

            Measure b1950{MeasureType::direction, MeasureRef{DirectionRef::b1950, std::nullopt},
                          DirectionValue{deg_to_rad(s.b1950_ra_deg), deg_to_rad(s.b1950_dec_deg)}};

            auto to_j2000 = convert_measure(b1950, DirectionRef::j2000);
            const auto& jv = std::get<DirectionValue>(to_j2000.value);

            const double exp_ra = deg_to_rad(s.j2000_ra_deg);
            const double exp_dec = deg_to_rad(s.j2000_dec_deg);
            const double dra_arcsec = rad_to_arcsec(angle_diff_rad(jv.lon_rad, exp_ra));
            const double ddec_arcsec = rad_to_arcsec(jv.lat_rad - exp_dec);

            std::cout << "\n  sample " << (i + 1) << ":\n";
            std::cout << "    B1950 input: RA=" << s.b1950_ra_deg << " deg Dec=" << s.b1950_dec_deg
                      << " deg\n";
            std::cout << "    J2000 out:   RA=" << (jv.lon_rad * 180.0 / M_PI)
                      << " deg Dec=" << (jv.lat_rad * 180.0 / M_PI) << " deg\n";
            std::cout << "    residual:    dRA=" << dra_arcsec << "\" dDec=" << ddec_arcsec
                      << "\" (vs dM1950_2000 references)\n";

            // Keep tolerance modest; original and mini use slightly different paths.
            DEMO_CHECK(std::abs(dra_arcsec) < 10.0);
            DEMO_CHECK(std::abs(ddec_arcsec) < 10.0);

            auto back_to_b1950 = convert_measure(to_j2000, DirectionRef::b1950);
            const auto& bv = std::get<DirectionValue>(back_to_b1950.value);
            const double rb_ra_arcsec =
                rad_to_arcsec(angle_diff_rad(bv.lon_rad, deg_to_rad(s.b1950_ra_deg)));
            const double rb_dec_arcsec = rad_to_arcsec(bv.lat_rad - deg_to_rad(s.b1950_dec_deg));
            std::cout << "    round-trip:  dRA=" << rb_ra_arcsec << "\" dDec=" << rb_dec_arcsec
                      << "\"\n";
            DEMO_CHECK(std::abs(rb_ra_arcsec) < 10.0);
            DEMO_CHECK(std::abs(rb_dec_arcsec) < 10.0);
        }

        std::cout << "\n=== B1950 <-> J2000 demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
