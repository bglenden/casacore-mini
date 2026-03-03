// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

// demo_measure.cpp -- Phase 8: measure conversion transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/measures/Measures/test/tMeasure.cc
//     MEpoch::Ref tmref(MEpoch::TAI);
//     MEpoch::Convert tconv(tm2, tmref);
//     cout << "Converted " << tm2 << " to " << tmref << " as " << tconv() << endl;
//
//     MDirection::Convert toGal(lsr1900, MDirection::J2000);
//     cout << toGal().getValue() << endl;
//
//   Source: casacore-original/measures/Measures/test/tVelocityMachine.cc
//     VelocityMachine vm(frqref, Unit("GHz"), restfrq, velref, Unit("km/s"), frame);
//     cout << vm.makeVelocity(1.41) << endl;
//     cout << vm.makeFrequency(bck) << endl;
//
// This casacore-mini demo transliterates the same families of operations:
// epoch conversion, direction conversion, Doppler conversions, and position conversion.

#include <casacore_mini/measure_convert.hpp>
#include <casacore_mini/measure_types.hpp>
#include <casacore_mini/quantity.hpp>
#include <casacore_mini/velocity_machine.hpp>

#include "demo_check.hpp"
#include <cmath>
#include <iostream>

using namespace casacore_mini;

namespace {

bool near(double a, double b, double tol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

} // namespace

// ---------------------------------------------------------------------------
// Epoch conversions
// ---------------------------------------------------------------------------
static void demo_epoch() {
    std::cout << "\n=== Epoch Conversions ===\n";

    // MJD 59000.5 UTC
    Measure epoch_utc{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
                      EpochValue{59000.0, 0.5}};
    std::cout << "  Input: MJD 59000.5 UTC\n";

    // UTC -> TAI: +37 seconds (current leap second offset)
    auto tai = convert_measure(epoch_utc, EpochRef::tai);
    const auto& tai_val = std::get<EpochValue>(tai.value);
    double tai_offset_s = (tai_val.day + tai_val.fraction - 59000.5) * 86400.0;
    std::cout << "  UTC -> TAI: offset = " << tai_offset_s << " s (expected ~37.0 s)\n";
    DEMO_CHECK(near(tai_offset_s, 37.0, 1e-6));

    // UTC -> TT: +69.184 seconds (TAI + 32.184)
    auto tt = convert_measure(epoch_utc, EpochRef::tdt);
    const auto& tt_val = std::get<EpochValue>(tt.value);
    double tt_offset_s = (tt_val.day + tt_val.fraction - 59000.5) * 86400.0;
    std::cout << "  UTC -> TT:  offset = " << tt_offset_s << " s (expected ~69.184 s)\n";
    DEMO_CHECK(near(tt_offset_s, 69.184, 1e-4));

    // Round-trip TAI -> UTC
    auto back = convert_measure(tai, EpochRef::utc);
    const auto& back_val = std::get<EpochValue>(back.value);
    double back_mjd = back_val.day + back_val.fraction;
    std::cout << "  TAI -> UTC: MJD = " << back_mjd << " (expected 59000.5)\n";
    DEMO_CHECK(near(back_mjd, 59000.5, 1e-12));

    std::cout << "  [OK] Epoch conversions verified.\n";
}

// ---------------------------------------------------------------------------
// Direction conversions
// ---------------------------------------------------------------------------
static void demo_direction() {
    std::cout << "\n=== Direction Conversions ===\n";

    // (RA=180 deg, Dec=45 deg) J2000 -- using Quantity for natural units.
    Quantity ra(180.0, "deg");
    Quantity dec(45.0, "deg");
    Measure dir_j2k{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
                    DirectionValue{ra.get_value("rad"), dec.get_value("rad")}};
    std::cout << "  Input: J2000 (RA=180 deg, Dec=45 deg)\n";

    // J2000 -> Galactic
    auto gal = convert_measure(dir_j2k, DirectionRef::galactic);
    const auto& gal_val = std::get<DirectionValue>(gal.value);
    double gal_lon = Quantity(gal_val.lon_rad, "rad").get_value("deg");
    double gal_lat = Quantity(gal_val.lat_rad, "rad").get_value("deg");
    std::cout << "  J2000 -> Galactic: l=" << gal_lon << " deg, b=" << gal_lat << " deg\n";

    // Round-trip Galactic -> J2000
    auto back = convert_measure(gal, DirectionRef::j2000);
    const auto& back_val = std::get<DirectionValue>(back.value);
    double back_ra = Quantity(back_val.lon_rad, "rad").get_value("deg");
    double back_dec = Quantity(back_val.lat_rad, "rad").get_value("deg");
    std::cout << "  Galactic -> J2000: RA=" << back_ra << " deg, Dec=" << back_dec << " deg\n";
    DEMO_CHECK(near(back_ra, 180.0, 1e-6));
    DEMO_CHECK(near(back_dec, 45.0, 1e-6));

    std::cout << "  [OK] Direction conversions verified.\n";
}

// ---------------------------------------------------------------------------
// Doppler conversions (purely mathematical)
// ---------------------------------------------------------------------------
static void demo_doppler() {
    std::cout << "\n=== Doppler Conversions ===\n";

    // radio = 0.1 (10% of speed of light)
    double radio_val = 0.1;
    std::cout << "  Input: radio Doppler = " << radio_val << "\n";

    // radio -> z
    double z_val = radio_to_z(radio_val);
    std::cout << "  radio -> z    = " << z_val << "\n";
    // For radio: v/c = radio, z = 1/(1-v/c) - 1 = 1/(1-radio) - 1
    double expected_z = 1.0 / (1.0 - radio_val) - 1.0;
    DEMO_CHECK(near(z_val, expected_z, 1e-10));

    // radio -> beta
    double beta_val = radio_to_beta(radio_val);
    std::cout << "  radio -> beta = " << beta_val << "\n";

    // VelocityMachine wrapper: radio -> z
    VelocityMachine radio_to_z_m(DopplerRef::radio, DopplerRef::z);
    double z_via_machine = radio_to_z_m.convert(radio_val);
    std::cout << "  VelocityMachine(radio->z).convert(0.1) = " << z_via_machine << "\n";
    DEMO_CHECK(near(z_via_machine, z_val, 1e-10));

    // Round-trip: z -> radio
    VelocityMachine z_to_radio_m(DopplerRef::z, DopplerRef::radio);
    double radio_back = z_to_radio_m.convert(z_val);
    std::cout << "  z -> radio round-trip = " << radio_back << "\n";
    DEMO_CHECK(near(radio_back, radio_val, 1e-10));

    std::cout << "  [OK] Doppler conversions verified.\n";
}

// ---------------------------------------------------------------------------
// Position conversions
// ---------------------------------------------------------------------------
static void demo_position() {
    std::cout << "\n=== Position Conversions ===\n";

    // VLA ITRF position (approx)
    Measure pos_itrf{MeasureType::position, MeasureRef{PositionRef::itrf, std::nullopt},
                     PositionValue{-1601185.0, -5041978.0, 3554876.0}};
    std::cout << "  Input: ITRF (-1601185, -5041978, 3554876) m [VLA approx]\n";

    // ITRF -> WGS84
    auto wgs84 = convert_measure(pos_itrf, PositionRef::wgs84);
    const auto& wgs_val = std::get<PositionValue>(wgs84.value);
    // WGS84 stores (lon_rad, lat_rad, height_m).
    double lon_deg = Quantity(wgs_val.x_m, "rad").get_value("deg");
    double lat_deg = Quantity(wgs_val.y_m, "rad").get_value("deg");
    std::cout << "  ITRF -> WGS84: lon=" << lon_deg << " deg, lat=" << lat_deg
              << " deg, h=" << wgs_val.z_m << " m\n";

    // VLA is at approximately (lon=-107.6, lat=34.1)
    // The conversion maps the Cartesian (x,y,z) to geodetic (lon,lat,h).
    std::cout << "  (VLA is at roughly lon=-107.6, lat=34.1 deg)\n";

    std::cout << "  [OK] Position conversion executed.\n";
}

int main() {
    try {
        std::cout << "=== casacore-mini Demo: Measures (Phase 8) ===\n";

        demo_epoch();
        demo_direction();
        demo_doppler();
        demo_position();

        std::cout << "\n=== All Measure demos passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
