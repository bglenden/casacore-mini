#include "casacore_mini/velocity_machine.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

constexpr double kTol = 1.0e-10;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

bool test_radio_velocity_roundtrip() {
    using namespace casacore_mini;
    double radio = 0.01; // 1% radio Doppler
    [[maybe_unused]] double vel = radio_doppler_to_velocity(radio);
    assert(near(vel, kSpeedOfLight * 0.01));
    assert(near(velocity_to_radio_doppler(vel), radio));
    return true;
}

bool test_z_velocity_roundtrip() {
    using namespace casacore_mini;
    double z = 0.5;
    [[maybe_unused]] double vel = z_doppler_to_velocity(z);
    assert(near(vel, kSpeedOfLight * 0.5));
    assert(near(velocity_to_z_doppler(vel), z));
    return true;
}

bool test_beta_velocity_roundtrip() {
    using namespace casacore_mini;
    double beta = 0.3;
    [[maybe_unused]] double vel = beta_to_velocity(beta);
    assert(near(vel, kSpeedOfLight * 0.3));
    assert(near(velocity_to_beta(vel), beta));
    return true;
}

bool test_freq_radio_velocity() {
    using namespace casacore_mini;
    double rest = 1.42040575e9; // HI 21cm line
    double freq = 1.42e9;
    double vel = freq_ratio_to_radio_velocity(freq, rest);
    assert(vel > 0.0); // redshifted → positive radio velocity
    [[maybe_unused]] double freq_back = radio_velocity_to_freq(vel, rest);
    assert(near(freq_back, freq));
    return true;
}

bool test_freq_optical_velocity() {
    using namespace casacore_mini;
    double rest = 1.42040575e9;
    double freq = 1.42e9;
    double vel = freq_ratio_to_optical_velocity(freq, rest);
    assert(vel > 0.0); // redshifted
    [[maybe_unused]] double freq_back = optical_velocity_to_freq(vel, rest);
    assert(near(freq_back, freq));
    return true;
}

bool test_radio_z_conversion() {
    using namespace casacore_mini;
    // At low velocities, radio ≈ z.
    double radio = 0.001;
    [[maybe_unused]] double z = radio_to_z(radio);
    assert(near(z, radio, 1.0e-5)); // close for small values

    // Round-trip.
    assert(near(z_to_radio(z), radio));

    // At high z, they diverge.
    double high_z = 1.0;
    [[maybe_unused]] double high_radio = z_to_radio(high_z);
    assert(near(radio_to_z(high_radio), high_z));
    return true;
}

bool test_radio_beta_conversion() {
    using namespace casacore_mini;
    double radio = 0.1;
    [[maybe_unused]] double beta = radio_to_beta(radio);
    // For small values, radio ≈ beta ≈ z.
    assert(std::abs(beta - radio) < 0.01);

    // Round-trip.
    assert(near(beta_to_radio(beta), radio));
    return true;
}

bool test_zero_doppler() {
    using namespace casacore_mini;
    assert(near(radio_doppler_to_velocity(0.0), 0.0));
    assert(near(z_doppler_to_velocity(0.0), 0.0));
    assert(near(beta_to_velocity(0.0), 0.0));
    assert(near(radio_to_z(0.0), 0.0));
    assert(near(radio_to_beta(0.0), 0.0));
    return true;
}

bool test_velocity_machine_wrapper() {
    using namespace casacore_mini;
    VelocityMachine radio_to_z_machine(DopplerRef::radio, DopplerRef::z);
    double radio = 0.02;
    double z = radio_to_z_machine.convert(radio);
    assert(near(z, radio_to_z(radio)));

    VelocityMachine z_to_radio_machine(DopplerRef::z, DopplerRef::radio);
    [[maybe_unused]] double radio_back = z_to_radio_machine.convert(z);
    assert(near(radio_back, radio));
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

    std::cout << "velocity_machine_test\n";
    run("radio_velocity_roundtrip", test_radio_velocity_roundtrip);
    run("z_velocity_roundtrip", test_z_velocity_roundtrip);
    run("beta_velocity_roundtrip", test_beta_velocity_roundtrip);
    run("freq_radio_velocity", test_freq_radio_velocity);
    run("freq_optical_velocity", test_freq_optical_velocity);
    run("radio_z_conversion", test_radio_z_conversion);
    run("radio_beta_conversion", test_radio_beta_conversion);
    run("zero_doppler", test_zero_doppler);
    run("velocity_machine_wrapper", test_velocity_machine_wrapper);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
