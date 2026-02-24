#include "casacore_mini/stokes_coordinate.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

using namespace casacore_mini;

bool test_iquv_mapping() {
    StokesCoordinate sc({1, 2, 3, 4}); // I, Q, U, V

    auto w0 = sc.to_world({0.0});
    assert(w0[0] == 1.0); // I

    auto w1 = sc.to_world({1.0});
    assert(w1[0] == 2.0); // Q

    auto w3 = sc.to_world({3.0});
    assert(w3[0] == 4.0); // V

    auto p = sc.to_pixel({3.0}); // U -> pixel 2
    assert(std::abs(p[0] - 2.0) < 0.01);
    return true;
}

bool test_rr_ll_mapping() {
    StokesCoordinate sc({5, 8}); // RR, LL

    auto w0 = sc.to_world({0.0});
    assert(w0[0] == 5.0);

    auto w1 = sc.to_world({1.0});
    assert(w1[0] == 8.0);

    auto p = sc.to_pixel({5.0});
    assert(std::abs(p[0] - 0.0) < 0.01);
    return true;
}

bool test_record_roundtrip() {
    StokesCoordinate sc({1, 2, 3, 4});
    Record rec = sc.save();
    auto restored = StokesCoordinate::from_record(rec);

    assert(restored->stokes_values().size() == 4);
    assert(restored->stokes_values()[0] == 1);
    assert(restored->stokes_values()[3] == 4);

    auto w = restored->to_world({2.0});
    assert(w[0] == 3.0); // U
    return true;
}

bool test_out_of_range_throws() {
    StokesCoordinate sc({1, 2});
    try {
        (void)sc.to_world({5.0}); // out of range
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_code_not_found_throws() {
    StokesCoordinate sc({1, 2, 3, 4});
    try {
        (void)sc.to_pixel({99.0}); // not in list
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

    std::cout << "stokes_coordinate_test\n";
    run("iquv_mapping", test_iquv_mapping);
    run("rr_ll_mapping", test_rr_ll_mapping);
    run("record_roundtrip", test_record_roundtrip);
    run("out_of_range_throws", test_out_of_range_throws);
    run("code_not_found_throws", test_code_not_found_throws);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
