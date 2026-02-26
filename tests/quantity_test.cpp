#include "casacore_mini/quantity.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr double kTol = 1.0e-12;

[[maybe_unused]] bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

bool test_identity_conversion() {
    using namespace casacore_mini;
    Quantity q{1.5, "rad"};
    auto r = convert_quantity(q, "rad");
    assert(r.value == q.value);
    assert(r.get_unit() == "rad");
    return true;
}

bool test_angle_rad_deg() {
    using namespace casacore_mini;
    {
        Quantity q{M_PI, "rad"};
        auto r = convert_quantity(q, "deg");
        assert(near(r.value, 180.0));
        assert(r.get_unit() == "deg");
    }
    {
        Quantity q{90.0, "deg"};
        auto r = convert_quantity(q, "rad");
        assert(near(r.value, M_PI / 2.0));
    }
    return true;
}

bool test_angle_arcmin_arcsec() {
    using namespace casacore_mini;
    {
        Quantity q{60.0, "arcmin"};
        auto r = convert_quantity(q, "deg");
        assert(near(r.value, 1.0));
    }
    {
        Quantity q{3600.0, "arcsec"};
        auto r = convert_quantity(q, "deg");
        assert(near(r.value, 1.0));
    }
    return true;
}

bool test_time_conversions() {
    using namespace casacore_mini;
    {
        Quantity q{86400.0, "s"};
        auto r = convert_quantity(q, "d");
        assert(near(r.value, 1.0));
    }
    {
        Quantity q{1.0, "h"};
        auto r = convert_quantity(q, "s");
        assert(near(r.value, 3600.0));
    }
    {
        Quantity q{1.0, "min"};
        auto r = convert_quantity(q, "s");
        assert(near(r.value, 60.0));
    }
    return true;
}

bool test_length_conversions() {
    using namespace casacore_mini;
    {
        Quantity q{1000.0, "m"};
        auto r = convert_quantity(q, "km");
        assert(near(r.value, 1.0));
    }
    {
        Quantity q{2.5, "km"};
        auto r = convert_quantity(q, "m");
        assert(near(r.value, 2500.0));
    }
    return true;
}

bool test_frequency_conversions() {
    using namespace casacore_mini;
    {
        Quantity q{1.0e9, "Hz"};
        auto r = convert_quantity(q, "GHz");
        assert(near(r.value, 1.0));
    }
    {
        Quantity q{1.0, "GHz"};
        auto r = convert_quantity(q, "MHz");
        assert(near(r.value, 1000.0));
    }
    {
        Quantity q{1.0, "MHz"};
        auto r = convert_quantity(q, "kHz");
        assert(near(r.value, 1000.0));
    }
    return true;
}

bool test_velocity_conversions() {
    using namespace casacore_mini;
    {
        Quantity q{1000.0, "m/s"};
        auto r = convert_quantity(q, "km/s");
        assert(near(r.value, 1.0));
    }
    return true;
}

bool test_unknown_unit_rejection() {
    using namespace casacore_mini;
    try {
        (void)convert_quantity(Quantity{1.0, "furlongs"}, "m");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_incompatible_units() {
    using namespace casacore_mini;
    try {
        (void)convert_quantity(Quantity{1.0, "rad"}, "Hz");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_round_trip() {
    using namespace casacore_mini;
    Quantity q{3.14159, "rad"};
    auto deg = convert_quantity(q, "deg");
    auto back = convert_quantity(deg, "rad");
    assert(near(back.value, q.value));
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

    std::cout << "quantity_test\n";
    run("identity_conversion", test_identity_conversion);
    run("angle_rad_deg", test_angle_rad_deg);
    run("angle_arcmin_arcsec", test_angle_arcmin_arcsec);
    run("time_conversions", test_time_conversions);
    run("length_conversions", test_length_conversions);
    run("frequency_conversions", test_frequency_conversions);
    run("velocity_conversions", test_velocity_conversions);
    run("unknown_unit_rejection", test_unknown_unit_rejection);
    run("incompatible_units", test_incompatible_units);
    run("round_trip", test_round_trip);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
