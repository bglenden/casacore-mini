#include "casacore_mini/measure_record.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

constexpr double kTol = 1.0e-12;

bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// Round-trip: all 9 measure types
// ---------------------------------------------------------------------------

bool test_epoch_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.0, 0.5}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::epoch);
    assert(std::holds_alternative<EpochRef>(m2.ref.ref_type));
    assert(std::get<EpochRef>(m2.ref.ref_type) == EpochRef::utc);
    const auto& ev = std::get<EpochValue>(m2.value);
    assert(near(ev.day, 59000.0));
    assert(near(ev.fraction, 0.5));
    return true;
}

bool test_direction_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::j2000, std::nullopt},
              DirectionValue{1.234, -0.567}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::direction);
    assert(std::get<DirectionRef>(m2.ref.ref_type) == DirectionRef::j2000);
    const auto& dv = std::get<DirectionValue>(m2.value);
    assert(near(dv.lon_rad, 1.234));
    assert(near(dv.lat_rad, -0.567));
    return true;
}

bool test_position_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::position, MeasureRef{PositionRef::itrf, std::nullopt},
              PositionValue{-2390490.0, 5564764.0, -1462209.0}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::position);
    const auto& pv = std::get<PositionValue>(m2.value);
    assert(near(pv.x_m, -2390490.0));
    assert(near(pv.y_m, 5564764.0));
    assert(near(pv.z_m, -1462209.0));
    return true;
}

bool test_frequency_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::frequency, MeasureRef{FrequencyRef::lsrk, std::nullopt},
              FrequencyValue{1.42040575e9}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::frequency);
    const auto& fv = std::get<FrequencyValue>(m2.value);
    assert(near(fv.hz, 1.42040575e9));
    return true;
}

bool test_doppler_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt},
              DopplerValue{0.001}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::doppler);
    assert(std::get<DopplerRef>(m2.ref.ref_type) == DopplerRef::radio);
    const auto& dv = std::get<DopplerValue>(m2.value);
    assert(near(dv.ratio, 0.001));
    return true;
}

bool test_radial_velocity_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::radial_velocity, MeasureRef{RadialVelocityRef::bary, std::nullopt},
              RadialVelocityValue{30000.0}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::radial_velocity);
    const auto& rv = std::get<RadialVelocityValue>(m2.value);
    assert(near(rv.mps, 30000.0));
    return true;
}

bool test_baseline_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::baseline, MeasureRef{BaselineRef::j2000, std::nullopt},
              BaselineValue{100.0, 200.0, 300.0}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::baseline);
    const auto& bv = std::get<BaselineValue>(m2.value);
    assert(near(bv.x_m, 100.0));
    assert(near(bv.y_m, 200.0));
    assert(near(bv.z_m, 300.0));
    return true;
}

bool test_uvw_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt},
              UvwValue{10.5, 20.5, 30.5}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::uvw);
    const auto& uv = std::get<UvwValue>(m2.value);
    assert(near(uv.u_m, 10.5));
    assert(near(uv.v_m, 20.5));
    assert(near(uv.w_m, 30.5));
    return true;
}

bool test_earth_magnetic_round_trip() {
    using namespace casacore_mini;
    Measure m{MeasureType::earth_magnetic, MeasureRef{EarthMagneticRef::igrf, std::nullopt},
              EarthMagneticValue{2.0e-5, 3.0e-5, 4.0e-5}};
    Record rec = measure_to_record(m);
    Measure m2 = measure_from_record(rec);
    assert(m2.type == MeasureType::earth_magnetic);
    assert(std::get<EarthMagneticRef>(m2.ref.ref_type) == EarthMagneticRef::igrf);
    const auto& emv = std::get<EarthMagneticValue>(m2.value);
    assert(near(emv.x_t, 2.0e-5));
    assert(near(emv.y_t, 3.0e-5));
    assert(near(emv.z_t, 4.0e-5));
    return true;
}

// ---------------------------------------------------------------------------
// Offset measure (recursive)
// ---------------------------------------------------------------------------

bool test_offset_round_trip() {
    using namespace casacore_mini;
    auto offset = std::make_shared<const Measure>(Measure{
        MeasureType::epoch, MeasureRef{EpochRef::tai, std::nullopt}, EpochValue{50000.0, 0.0}});
    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, offset}, EpochValue{59000.0, 0.5}};
    Record rec = measure_to_record(m);

    // Verify offset sub-record exists.
    const auto* offset_field = rec.find("offset");
    assert(offset_field != nullptr);

    Measure m2 = measure_from_record(rec);
    assert(m2.ref.offset.has_value());
    assert(*m2.ref.offset != nullptr);
    const auto& off = **m2.ref.offset;
    assert(off.type == MeasureType::epoch);
    assert(std::get<EpochRef>(off.ref.ref_type) == EpochRef::tai);
    const auto& oev = std::get<EpochValue>(off.value);
    assert(near(oev.day, 50000.0));
    return true;
}

// ---------------------------------------------------------------------------
// Record field structure
// ---------------------------------------------------------------------------

bool test_record_field_names() {
    using namespace casacore_mini;
    Measure m{MeasureType::direction, MeasureRef{DirectionRef::galactic, std::nullopt},
              DirectionValue{3.14, 0.0}};
    Record rec = measure_to_record(m);

    // Must have type, refer, m0, m1.
    assert(rec.find("type") != nullptr);
    assert(rec.find("refer") != nullptr);
    assert(rec.find("m0") != nullptr);
    assert(rec.find("m1") != nullptr);
    assert(rec.find("m2") == nullptr);     // 2-component, no m2
    assert(rec.find("offset") == nullptr); // no offset

    // Check type string value.
    const auto* type_val = rec.find("type");
    const auto* sp = std::get_if<std::string>(&type_val->storage());
    assert(sp != nullptr);
    assert(*sp == "direction");

    return true;
}

// ---------------------------------------------------------------------------
// Malformed input
// ---------------------------------------------------------------------------

bool test_missing_type() {
    using namespace casacore_mini;
    Record rec;
    rec.set("refer", RecordValue(std::string("UTC")));
    rec.set("m0", RecordValue::from_record(Record{}));
    try {
        (void)measure_from_record(rec);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_unknown_refer() {
    using namespace casacore_mini;
    Record rec;
    rec.set("type", RecordValue(std::string("epoch")));
    rec.set("refer", RecordValue(std::string("BOGUS_REF")));
    Record m0;
    m0.set("value", RecordValue(59000.0));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));
    Record m1;
    m1.set("value", RecordValue(0.5));
    m1.set("unit", RecordValue(std::string("d")));
    rec.set("m1", RecordValue::from_record(std::move(m1)));
    try {
        (void)measure_from_record(rec);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_missing_m1_for_2component() {
    using namespace casacore_mini;
    Record rec;
    rec.set("type", RecordValue(std::string("epoch")));
    rec.set("refer", RecordValue(std::string("UTC")));
    Record m0;
    m0.set("value", RecordValue(59000.0));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));
    // Missing m1 — should throw.
    try {
        (void)measure_from_record(rec);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_unknown_type() {
    using namespace casacore_mini;
    Record rec;
    rec.set("type", RecordValue(std::string("bogus_type")));
    rec.set("refer", RecordValue(std::string("UTC")));
    Record m0;
    m0.set("value", RecordValue(0.0));
    rec.set("m0", RecordValue::from_record(std::move(m0)));
    try {
        (void)measure_from_record(rec);
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

    std::cout << "measure_record_test\n";
    run("epoch_round_trip", test_epoch_round_trip);
    run("direction_round_trip", test_direction_round_trip);
    run("position_round_trip", test_position_round_trip);
    run("frequency_round_trip", test_frequency_round_trip);
    run("doppler_round_trip", test_doppler_round_trip);
    run("radial_velocity_round_trip", test_radial_velocity_round_trip);
    run("baseline_round_trip", test_baseline_round_trip);
    run("uvw_round_trip", test_uvw_round_trip);
    run("earth_magnetic_round_trip", test_earth_magnetic_round_trip);
    run("offset_round_trip", test_offset_round_trip);
    run("record_field_names", test_record_field_names);
    run("missing_type", test_missing_type);
    run("unknown_refer", test_unknown_refer);
    run("missing_m1_for_2component", test_missing_m1_for_2component);
    run("unknown_type", test_unknown_type);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
