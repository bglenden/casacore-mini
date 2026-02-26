#include "casacore_mini/measure_types.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// MeasureType round-trips
// ---------------------------------------------------------------------------

bool test_measure_type_strings() {
    using namespace casacore_mini;
    // All 9 types round-trip through string conversion.
    assert(string_to_measure_type("epoch") == MeasureType::epoch);
    assert(string_to_measure_type("direction") == MeasureType::direction);
    assert(string_to_measure_type("position") == MeasureType::position);
    assert(string_to_measure_type("frequency") == MeasureType::frequency);
    assert(string_to_measure_type("doppler") == MeasureType::doppler);
    assert(string_to_measure_type("radialvelocity") == MeasureType::radial_velocity);
    assert(string_to_measure_type("baseline") == MeasureType::baseline);
    assert(string_to_measure_type("uvw") == MeasureType::uvw);
    assert(string_to_measure_type("earthmagnetic") == MeasureType::earth_magnetic);

    // Case-insensitive.
    assert(string_to_measure_type("EPOCH") == MeasureType::epoch);
    assert(string_to_measure_type("Direction") == MeasureType::direction);
    assert(string_to_measure_type("RadialVelocity") == MeasureType::radial_velocity);

    // to_string round-trip.
    for ([[maybe_unused]] auto t :
         {MeasureType::epoch, MeasureType::direction, MeasureType::position, MeasureType::frequency,
          MeasureType::doppler, MeasureType::radial_velocity, MeasureType::baseline,
          MeasureType::uvw, MeasureType::earth_magnetic}) {
        assert(string_to_measure_type(measure_type_to_string(t)) == t);
    }

    // Unknown type throws.
    try {
        (void)string_to_measure_type("bogus");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }

    return true;
}

// ---------------------------------------------------------------------------
// EpochRef
// ---------------------------------------------------------------------------

bool test_epoch_ref() {
    using namespace casacore_mini;
    // Canonical round-trips.
    for ([[maybe_unused]] auto r : {EpochRef::last, EpochRef::lmst, EpochRef::gmst1, EpochRef::gast,
                                    EpochRef::ut1, EpochRef::ut2, EpochRef::utc, EpochRef::tai,
                                    EpochRef::tdt, EpochRef::tcg, EpochRef::tdb, EpochRef::tcb}) {
        assert(string_to_epoch_ref(epoch_ref_to_string(r)) == r);
    }

    // Synonyms.
    assert(string_to_epoch_ref("IAT") == EpochRef::tai);
    assert(string_to_epoch_ref("TT") == EpochRef::tdt);
    assert(string_to_epoch_ref("ET") == EpochRef::tdt);
    assert(string_to_epoch_ref("UT") == EpochRef::ut1);
    assert(string_to_epoch_ref("GMST") == EpochRef::gmst1);

    // Case-insensitive.
    assert(string_to_epoch_ref("utc") == EpochRef::utc);
    assert(string_to_epoch_ref("Tai") == EpochRef::tai);

    // Unknown throws.
    try {
        (void)string_to_epoch_ref("BOGUS");
        assert(false);
    } catch (const std::invalid_argument&) { // expected
    }

    return true;
}

// ---------------------------------------------------------------------------
// DirectionRef
// ---------------------------------------------------------------------------

bool test_direction_ref() {
    using namespace casacore_mini;
    for ([[maybe_unused]] auto r :
         {DirectionRef::j2000,    DirectionRef::jmean,     DirectionRef::jtrue,
          DirectionRef::app,      DirectionRef::b1950,     DirectionRef::b1950_vla,
          DirectionRef::bmean,    DirectionRef::btrue,     DirectionRef::galactic,
          DirectionRef::hadec,    DirectionRef::azel,      DirectionRef::azelsw,
          DirectionRef::azelgeo,  DirectionRef::azelswgeo, DirectionRef::jnat,
          DirectionRef::ecliptic, DirectionRef::mecliptic, DirectionRef::tecliptic,
          DirectionRef::supergal, DirectionRef::itrf,      DirectionRef::topo,
          DirectionRef::icrs}) {
        assert(string_to_direction_ref(direction_ref_to_string(r)) == r);
    }

    // Synonyms.
    assert(string_to_direction_ref("AZELNE") == DirectionRef::azel);
    assert(string_to_direction_ref("AZELNEGEO") == DirectionRef::azelgeo);

    return true;
}

// ---------------------------------------------------------------------------
// PositionRef
// ---------------------------------------------------------------------------

bool test_position_ref() {
    using namespace casacore_mini;
    assert(string_to_position_ref("ITRF") == PositionRef::itrf);
    assert(string_to_position_ref("WGS84") == PositionRef::wgs84);
    assert(string_to_position_ref(position_ref_to_string(PositionRef::itrf)) == PositionRef::itrf);
    assert(string_to_position_ref(position_ref_to_string(PositionRef::wgs84)) ==
           PositionRef::wgs84);
    return true;
}

// ---------------------------------------------------------------------------
// FrequencyRef
// ---------------------------------------------------------------------------

bool test_frequency_ref() {
    using namespace casacore_mini;
    for ([[maybe_unused]] auto r :
         {FrequencyRef::rest, FrequencyRef::lsrk, FrequencyRef::lsrd, FrequencyRef::bary,
          FrequencyRef::geo, FrequencyRef::topo, FrequencyRef::galacto, FrequencyRef::lgroup,
          FrequencyRef::cmb}) {
        assert(string_to_frequency_ref(frequency_ref_to_string(r)) == r);
    }
    // LSR synonym.
    assert(string_to_frequency_ref("LSR") == FrequencyRef::lsrk);
    return true;
}

// ---------------------------------------------------------------------------
// DopplerRef
// ---------------------------------------------------------------------------

bool test_doppler_ref() {
    using namespace casacore_mini;
    for ([[maybe_unused]] auto r : {DopplerRef::radio, DopplerRef::z, DopplerRef::ratio,
                                    DopplerRef::beta, DopplerRef::gamma}) {
        assert(string_to_doppler_ref(doppler_ref_to_string(r)) == r);
    }
    // Synonyms.
    assert(string_to_doppler_ref("OPTICAL") == DopplerRef::z);
    assert(string_to_doppler_ref("RELATIVISTIC") == DopplerRef::beta);
    return true;
}

// ---------------------------------------------------------------------------
// RadialVelocityRef
// ---------------------------------------------------------------------------

bool test_radial_velocity_ref() {
    using namespace casacore_mini;
    for ([[maybe_unused]] auto r :
         {RadialVelocityRef::lsrk, RadialVelocityRef::lsrd, RadialVelocityRef::bary,
          RadialVelocityRef::geo, RadialVelocityRef::topo, RadialVelocityRef::galacto,
          RadialVelocityRef::lgroup, RadialVelocityRef::cmb}) {
        assert(string_to_radial_velocity_ref(radial_velocity_ref_to_string(r)) == r);
    }
    assert(string_to_radial_velocity_ref("LSR") == RadialVelocityRef::lsrk);
    return true;
}

// ---------------------------------------------------------------------------
// BaselineRef, UvwRef, EarthMagneticRef (spot checks)
// ---------------------------------------------------------------------------

bool test_baseline_ref() {
    using namespace casacore_mini;
    assert(string_to_baseline_ref("J2000") == BaselineRef::j2000);
    assert(string_to_baseline_ref("GALACTIC") == BaselineRef::galactic);
    assert(string_to_baseline_ref("AZELNE") == BaselineRef::azel);
    assert(string_to_baseline_ref(baseline_ref_to_string(BaselineRef::icrs)) == BaselineRef::icrs);
    return true;
}

bool test_uvw_ref() {
    using namespace casacore_mini;
    assert(string_to_uvw_ref("J2000") == UvwRef::j2000);
    assert(string_to_uvw_ref("ITRF") == UvwRef::itrf);
    assert(string_to_uvw_ref(uvw_ref_to_string(UvwRef::topo)) == UvwRef::topo);
    return true;
}

bool test_earth_magnetic_ref() {
    using namespace casacore_mini;
    assert(string_to_earth_magnetic_ref("IGRF") == EarthMagneticRef::igrf);
    assert(string_to_earth_magnetic_ref("J2000") == EarthMagneticRef::j2000);
    assert(string_to_earth_magnetic_ref(earth_magnetic_ref_to_string(EarthMagneticRef::igrf)) ==
           EarthMagneticRef::igrf);
    return true;
}

// ---------------------------------------------------------------------------
// measure_ref_to_string
// ---------------------------------------------------------------------------

bool test_measure_ref_to_string() {
    using namespace casacore_mini;
    MeasureRefType ref = EpochRef::utc;
    assert(measure_ref_to_string(ref) == "UTC");

    ref = DirectionRef::j2000;
    assert(measure_ref_to_string(ref) == "J2000");

    ref = FrequencyRef::lsrk;
    assert(measure_ref_to_string(ref) == "LSRK");

    return true;
}

// ---------------------------------------------------------------------------
// Value struct construction and equality
// ---------------------------------------------------------------------------

bool test_value_structs() {
    using namespace casacore_mini;

    [[maybe_unused]] EpochValue e1{59000.0, 0.5};
    [[maybe_unused]] EpochValue e2{59000.0, 0.5};
    assert(e1 == e2);
    e2.fraction = 0.6;
    assert(!(e1 == e2));

    [[maybe_unused]] DirectionValue d1{1.0, 2.0};
    [[maybe_unused]] DirectionValue d2{1.0, 2.0};
    assert(d1 == d2);

    [[maybe_unused]] PositionValue p1{100.0, 200.0, 300.0};
    assert(p1.x_m == 100.0);

    [[maybe_unused]] FrequencyValue f1{1.4e9};
    assert(f1.hz == 1.4e9);

    return true;
}

// ---------------------------------------------------------------------------
// Measure construction and equality
// ---------------------------------------------------------------------------

bool test_measure_equality() {
    using namespace casacore_mini;

    Measure m1{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
               EpochValue{59000.0, 0.5}};
    Measure m2{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
               EpochValue{59000.0, 0.5}};
    assert(m1 == m2);

    Measure m3{MeasureType::epoch, MeasureRef{EpochRef::tai, std::nullopt},
               EpochValue{59000.0, 0.5}};
    assert(!(m1 == m3));

    return true;
}

// ---------------------------------------------------------------------------
// default_ref_for_type
// ---------------------------------------------------------------------------

bool test_default_ref() {
    using namespace casacore_mini;
    auto ref = default_ref_for_type(MeasureType::epoch);
    assert(std::holds_alternative<EpochRef>(ref));
    assert(std::get<EpochRef>(ref) == EpochRef::utc);

    ref = default_ref_for_type(MeasureType::direction);
    assert(std::get<DirectionRef>(ref) == DirectionRef::j2000);

    ref = default_ref_for_type(MeasureType::position);
    assert(std::get<PositionRef>(ref) == PositionRef::itrf);

    ref = default_ref_for_type(MeasureType::frequency);
    assert(std::get<FrequencyRef>(ref) == FrequencyRef::rest);

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

    std::cout << "measure_types_test\n";
    run("measure_type_strings", test_measure_type_strings);
    run("epoch_ref", test_epoch_ref);
    run("direction_ref", test_direction_ref);
    run("position_ref", test_position_ref);
    run("frequency_ref", test_frequency_ref);
    run("doppler_ref", test_doppler_ref);
    run("radial_velocity_ref", test_radial_velocity_ref);
    run("baseline_ref", test_baseline_ref);
    run("uvw_ref", test_uvw_ref);
    run("earth_magnetic_ref", test_earth_magnetic_ref);
    run("measure_ref_to_string", test_measure_ref_to_string);
    run("value_structs", test_value_structs);
    run("measure_equality", test_measure_equality);
    run("default_ref", test_default_ref);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
