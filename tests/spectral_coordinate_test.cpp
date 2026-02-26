#include "casacore_mini/spectral_coordinate.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-10;

[[maybe_unused]] bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

bool test_linear_channel_mapping() {
    SpectralCoordinate sc(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0, 1.42e9);

    // Channel 0 -> crval
    auto w0 = sc.to_world({0.0});
    assert(near(w0[0], 1.42e9));

    // Channel 10 -> crval + 10*cdelt
    auto w10 = sc.to_world({10.0});
    assert(near(w10[0], 1.42e9 + 10.0 * 1e6));

    // Round-trip.
    auto p10 = sc.to_pixel(w10);
    assert(near(p10[0], 10.0));
    return true;
}

bool test_with_crpix_offset() {
    SpectralCoordinate sc(FrequencyRef::topo, 1.0e9, 500e3, 5.0);

    // At crpix=5 -> crval
    auto w5 = sc.to_world({5.0});
    assert(near(w5[0], 1.0e9));

    // At pixel 0 -> crval - 5*cdelt
    auto w0 = sc.to_world({0.0});
    assert(near(w0[0], 1.0e9 - 5.0 * 500e3));
    return true;
}

bool test_record_roundtrip() {
    SpectralCoordinate sc(FrequencyRef::bary, 1.42e9, 1e6, 0.0, 1.42e9);
    Record rec = sc.save();
    auto restored = SpectralCoordinate::from_record(rec);

    assert(restored->ref_frame() == FrequencyRef::bary);
    assert(near(restored->rest_freq_hz(), 1.42e9));

    auto w1 = sc.to_world({5.0});
    auto w2 = restored->to_world({5.0});
    assert(near(w1[0], w2[0]));
    return true;
}

bool test_clone() {
    SpectralCoordinate sc(FrequencyRef::lsrk, 1e9, 1e6, 0.0);
    auto cloned = sc.clone();
    assert(cloned->type() == CoordinateType::spectral);

    auto w1 = sc.to_world({3.0});
    auto w2 = cloned->to_world({3.0});
    assert(near(w1[0], w2[0]));
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

    std::cout << "spectral_coordinate_test\n";
    run("linear_channel_mapping", test_linear_channel_mapping);
    run("with_crpix_offset", test_with_crpix_offset);
    run("record_roundtrip", test_record_roundtrip);
    run("clone", test_clone);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
