#include "casacore_mini/projection.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace casacore_mini;

bool test_all_types_to_string() {
    // Every enum value should round-trip through string conversion.
    ProjectionType types[] = {
        ProjectionType::azp, ProjectionType::szp, ProjectionType::tan, ProjectionType::stg,
        ProjectionType::sin, ProjectionType::arc, ProjectionType::zpn, ProjectionType::zea,
        ProjectionType::air, ProjectionType::cyp, ProjectionType::cea, ProjectionType::car,
        ProjectionType::mer, ProjectionType::cop, ProjectionType::cod, ProjectionType::coe,
        ProjectionType::coo, ProjectionType::sfl, ProjectionType::par, ProjectionType::mol,
        ProjectionType::ait, ProjectionType::bon, ProjectionType::pco, ProjectionType::tsc,
        ProjectionType::csc, ProjectionType::qsc, ProjectionType::hpx, ProjectionType::xph,
    };

    for (auto t : types) {
        std::string s = projection_type_to_string(t);
        assert(s.size() == 3);
        [[maybe_unused]] ProjectionType rt = string_to_projection_type(s);
        assert(rt == t);
    }
    return true;
}

bool test_case_insensitive() {
    assert(string_to_projection_type("tan") == ProjectionType::tan);
    assert(string_to_projection_type("TAN") == ProjectionType::tan);
    assert(string_to_projection_type("Tan") == ProjectionType::tan);
    assert(string_to_projection_type("sin") == ProjectionType::sin);
    assert(string_to_projection_type("AIT") == ProjectionType::ait);
    return true;
}

bool test_unknown_throws() {
    try {
        (void)string_to_projection_type("XYZ");
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

bool test_parameter_counts() {
    assert(projection_parameter_count(ProjectionType::tan) == 0);
    assert(projection_parameter_count(ProjectionType::sin) == 2);
    assert(projection_parameter_count(ProjectionType::zpn) == 20);
    assert(projection_parameter_count(ProjectionType::azp) == 2);
    assert(projection_parameter_count(ProjectionType::car) == 0);
    assert(projection_parameter_count(ProjectionType::cea) == 1);
    return true;
}

bool test_projection_struct() {
    Projection p{ProjectionType::sin, {0.0, 0.0}};
    Projection q{ProjectionType::sin, {0.0, 0.0}};
    assert(p == q);

    Projection r{ProjectionType::tan, {}};
    assert(!(p == r));
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

    std::cout << "projection_test\n";
    run("all_types_to_string", test_all_types_to_string);
    run("case_insensitive", test_case_insensitive);
    run("unknown_throws", test_unknown_throws);
    run("parameter_counts", test_parameter_counts);
    run("projection_struct", test_projection_struct);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
