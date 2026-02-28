#include "casacore_mini/measure_convert.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/quantity.hpp"
#include "casacore_mini/velocity_machine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

using namespace casacore_mini;

bool test_utc_to_tai_no_reference_data() {
    // UTC->TAI is a fixed offset (leap seconds) that should work
    // without external reference data files.
    Measure epoch;
    epoch.type = MeasureType::epoch;
    epoch.ref.ref_type = EpochRef::utc;
    epoch.value = EpochValue{59000.5, 0.0};

    MeasureFrame frame;
    auto converted = convert_measure(epoch, EpochRef::tai, frame);

    auto ev = std::get<EpochValue>(converted.value);
    double actual_tai = ev.day + ev.fraction;
    double expected_tai = 59000.5 + 37.0 / 86400.0;
    if (std::abs(actual_tai - expected_tai) >= 1e-8) return false;
    return true;
}

bool test_utc_to_tdt_no_reference_data() {
    // UTC->TDT goes through TAI: TDT = TAI + 32.184s
    Measure epoch;
    epoch.type = MeasureType::epoch;
    epoch.ref.ref_type = EpochRef::utc;
    epoch.value = EpochValue{59000.5, 0.0};

    MeasureFrame frame;
    auto converted = convert_measure(epoch, EpochRef::tdt, frame);

    auto ev = std::get<EpochValue>(converted.value);
    double actual_tdt = ev.day + ev.fraction;
    double expected_tdt = 59000.5 + (37.0 + 32.184) / 86400.0;
    if (std::abs(actual_tdt - expected_tdt) >= 1e-8) return false;
    return true;
}

bool test_j2000_to_galactic_no_frame() {
    // J2000->GALACTIC is a fixed rotation, no frame needed.
    Measure dir;
    dir.type = MeasureType::direction;
    dir.ref.ref_type = DirectionRef::j2000;
    dir.value = DirectionValue{M_PI, M_PI / 4.0}; // 180 deg, 45 deg

    MeasureFrame frame;
    auto converted = convert_measure(dir, DirectionRef::galactic, frame);

    auto dv = std::get<DirectionValue>(converted.value);
    if (!std::isfinite(dv.lon_rad)) return false;
    if (!std::isfinite(dv.lat_rad)) return false;
    // Galactic lat should be > 0 for this direction (high galactic latitude)
    if (dv.lat_rad <= 0.5) return false; // roughly > 30 deg
    return true;
}

bool test_doppler_conversion_no_deps() {
    // Doppler conversions are purely mathematical.
    constexpr double kZ = 0.1;
    double radio = z_to_radio(kZ);
    double back = radio_to_z(radio);
    if (std::abs(back - kZ) >= 1e-12) return false;
    return true;
}

bool test_velocity_machine_no_deps() {
    // Freq <-> velocity is purely mathematical.
    constexpr double kRestFreq = 1.42e9;      // Hz
    constexpr double kObsFreq = 1.42e9 / 1.1; // z=0.1

    double vel = freq_ratio_to_optical_velocity(kObsFreq, kRestFreq);
    double freq_back = optical_velocity_to_freq(vel, kRestFreq);
    if (std::abs(freq_back - kObsFreq) / kObsFreq >= 1e-12) return false;
    return true;
}

bool test_unsupported_frame_pair_throws() {
    // J2000->APP requires epoch in frame; empty frame should throw.
    Measure dir;
    dir.type = MeasureType::direction;
    dir.ref.ref_type = DirectionRef::j2000;
    dir.value = DirectionValue{0.0, 0.0};

    MeasureFrame frame; // empty

    try {
        (void)convert_measure(dir, DirectionRef::app, frame);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    } catch (const std::runtime_error&) {
        return true;
    }
}

bool test_quantity_unknown_unit() {
    try {
        (void)convert_quantity(Quantity{1.0, "furlongs"}, "m");
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
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

    std::cout << "reference_data_absence_test\n";
    run("utc_to_tai_no_reference_data", test_utc_to_tai_no_reference_data);
    run("utc_to_tdt_no_reference_data", test_utc_to_tdt_no_reference_data);
    run("j2000_to_galactic_no_frame", test_j2000_to_galactic_no_frame);
    run("doppler_conversion_no_deps", test_doppler_conversion_no_deps);
    run("velocity_machine_no_deps", test_velocity_machine_no_deps);
    run("unsupported_frame_pair_throws", test_unsupported_frame_pair_throws);
    run("quantity_unknown_unit", test_quantity_unknown_unit);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
