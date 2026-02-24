#include "casacore_mini/measure_convert.hpp"
#include "casacore_mini/measure_types.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

constexpr double kTol = 1.0e-10;

bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// Doppler conversions (purely mathematical)
// ---------------------------------------------------------------------------

bool test_doppler_radio_to_z_roundtrip() {
    using namespace casacore_mini;
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt},
              DopplerValue{0.01}};
    auto z = convert_measure(m, DopplerRef::z);
    auto back = convert_measure(z, DopplerRef::radio);
    const auto& dv = std::get<DopplerValue>(back.value);
    assert(near(dv.ratio, 0.01));
    return true;
}

bool test_doppler_radio_to_beta_roundtrip() {
    using namespace casacore_mini;
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt}, DopplerValue{0.1}};
    auto beta = convert_measure(m, DopplerRef::beta);
    auto back = convert_measure(beta, DopplerRef::radio);
    const auto& dv = std::get<DopplerValue>(back.value);
    assert(near(dv.ratio, 0.1));
    return true;
}

bool test_doppler_radio_to_ratio() {
    using namespace casacore_mini;
    // radio = 1 - f/f0 = 0.01, so ratio = f/f0 = 0.99
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt},
              DopplerValue{0.01}};
    auto ratio = convert_measure(m, DopplerRef::ratio);
    const auto& dv = std::get<DopplerValue>(ratio.value);
    assert(near(dv.ratio, 0.99));
    return true;
}

bool test_doppler_radio_to_gamma_roundtrip() {
    using namespace casacore_mini;
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt}, DopplerValue{0.3}};
    auto gamma = convert_measure(m, DopplerRef::gamma);
    // gamma should be > 1
    const auto& gv = std::get<DopplerValue>(gamma.value);
    assert(gv.ratio > 1.0);

    auto back = convert_measure(gamma, DopplerRef::radio);
    const auto& dv = std::get<DopplerValue>(back.value);
    assert(near(dv.ratio, 0.3));
    return true;
}

bool test_doppler_z_to_beta_consistency() {
    using namespace casacore_mini;
    // z=1 should give beta = 3/5 = 0.6 (special relativity)
    // (1+z)^2 = 4, beta = (4-1)/(4+1) = 3/5
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::z, std::nullopt}, DopplerValue{1.0}};
    auto beta_m = convert_measure(m, DopplerRef::beta);
    const auto& bv = std::get<DopplerValue>(beta_m.value);
    assert(near(bv.ratio, 0.6));
    return true;
}

bool test_doppler_identity() {
    using namespace casacore_mini;
    Measure m{MeasureType::doppler, MeasureRef{DopplerRef::radio, std::nullopt},
              DopplerValue{0.05}};
    auto same = convert_measure(m, DopplerRef::radio);
    const auto& dv = std::get<DopplerValue>(same.value);
    assert(near(dv.ratio, 0.05));
    return true;
}

// ---------------------------------------------------------------------------
// Frequency conversions
// ---------------------------------------------------------------------------

bool test_frequency_identity() {
    using namespace casacore_mini;
    Measure m{MeasureType::frequency, MeasureRef{FrequencyRef::lsrk, std::nullopt},
              FrequencyValue{1.42e9}};
    auto same = convert_measure(m, FrequencyRef::lsrk);
    const auto& fv = std::get<FrequencyValue>(same.value);
    assert(near(fv.hz, 1.42e9));
    return true;
}

bool test_frequency_cross_frame_throws() {
    using namespace casacore_mini;
    Measure m{MeasureType::frequency, MeasureRef{FrequencyRef::rest, std::nullopt},
              FrequencyValue{1.42e9}};
    try {
        (void)convert_measure(m, FrequencyRef::lsrk);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) {
        // Expected: cross-frame needs observer velocity correction.
    }
    return true;
}

// ---------------------------------------------------------------------------
// Radial velocity conversions
// ---------------------------------------------------------------------------

bool test_radial_velocity_identity() {
    using namespace casacore_mini;
    Measure m{MeasureType::radial_velocity, MeasureRef{RadialVelocityRef::lsrk, std::nullopt},
              RadialVelocityValue{1500.0}};
    auto same = convert_measure(m, RadialVelocityRef::lsrk);
    const auto& rv = std::get<RadialVelocityValue>(same.value);
    assert(near(rv.mps, 1500.0));
    return true;
}

// ---------------------------------------------------------------------------
// Baseline conversions
// ---------------------------------------------------------------------------

bool test_baseline_j2000_to_galactic() {
    using namespace casacore_mini;
    Measure m{MeasureType::baseline, MeasureRef{BaselineRef::j2000, std::nullopt},
              BaselineValue{100.0, 200.0, 300.0}};
    auto gal = convert_measure(m, BaselineRef::galactic);
    // Round-trip back.
    auto back = convert_measure(gal, BaselineRef::j2000);
    const auto& bv = std::get<BaselineValue>(back.value);
    assert(near(bv.x_m, 100.0, 1e-6));
    assert(near(bv.y_m, 200.0, 1e-6));
    assert(near(bv.z_m, 300.0, 1e-6));
    return true;
}

// ---------------------------------------------------------------------------
// UVW conversions
// ---------------------------------------------------------------------------

bool test_uvw_j2000_to_galactic() {
    using namespace casacore_mini;
    Measure m{MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt},
              UvwValue{100.0, 200.0, 300.0}};
    auto gal = convert_measure(m, UvwRef::galactic);
    // Round-trip back.
    auto back = convert_measure(gal, UvwRef::j2000);
    const auto& uv = std::get<UvwValue>(back.value);
    assert(near(uv.u_m, 100.0, 1e-6));
    assert(near(uv.v_m, 200.0, 1e-6));
    assert(near(uv.w_m, 300.0, 1e-6));
    return true;
}

// ---------------------------------------------------------------------------
// EarthMagnetic conversions
// ---------------------------------------------------------------------------

bool test_earth_magnetic_frame_rotation() {
    using namespace casacore_mini;
    Measure m{MeasureType::earth_magnetic, MeasureRef{EarthMagneticRef::j2000, std::nullopt},
              EarthMagneticValue{1e-5, 2e-5, 3e-5}};
    auto gal = convert_measure(m, EarthMagneticRef::galactic);
    // Round-trip back.
    auto back = convert_measure(gal, EarthMagneticRef::j2000);
    const auto& ev = std::get<EarthMagneticValue>(back.value);
    assert(near(ev.x_t, 1e-5, 1e-6));
    assert(near(ev.y_t, 2e-5, 1e-6));
    assert(near(ev.z_t, 3e-5, 1e-6));
    return true;
}

bool test_earth_magnetic_igrf_throws() {
    using namespace casacore_mini;
    Measure m{MeasureType::earth_magnetic, MeasureRef{EarthMagneticRef::igrf, std::nullopt},
              EarthMagneticValue{1e-5, 2e-5, 3e-5}};
    try {
        (void)convert_measure(m, EarthMagneticRef::j2000);
        assert(false && "Should have thrown");
    } catch (const std::runtime_error&) {
        // Expected: IGRF not implemented.
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

    std::cout << "measure_convert_full_test\n";
    run("doppler_radio_to_z_roundtrip", test_doppler_radio_to_z_roundtrip);
    run("doppler_radio_to_beta_roundtrip", test_doppler_radio_to_beta_roundtrip);
    run("doppler_radio_to_ratio", test_doppler_radio_to_ratio);
    run("doppler_radio_to_gamma_roundtrip", test_doppler_radio_to_gamma_roundtrip);
    run("doppler_z_to_beta_consistency", test_doppler_z_to_beta_consistency);
    run("doppler_identity", test_doppler_identity);
    run("frequency_identity", test_frequency_identity);
    run("frequency_cross_frame_throws", test_frequency_cross_frame_throws);
    run("radial_velocity_identity", test_radial_velocity_identity);
    run("baseline_j2000_to_galactic", test_baseline_j2000_to_galactic);
    run("uvw_j2000_to_galactic", test_uvw_j2000_to_galactic);
    run("earth_magnetic_frame_rotation", test_earth_magnetic_frame_rotation);
    run("earth_magnetic_igrf_throws", test_earth_magnetic_igrf_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
