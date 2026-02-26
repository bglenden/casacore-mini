#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/gaussian_convert.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

using namespace casacore_mini;

constexpr double kTol = 0.05; // 5% tolerance for numerical Jacobian
constexpr double kDeg2Rad = M_PI / 180.0;
constexpr double kArcsec2Rad = kDeg2Rad / 3600.0;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0e-10, std::abs(a));
}

bool test_circular_gaussian() {
    // 1 arcsec/pixel, no rotation -> circular Gaussian should stay circular.
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           45.0 * kDeg2Rad, -kArcsec2Rad, kArcsec2Rad, {}, 100.0, 100.0);

    double major = 10.0 * kArcsec2Rad;
    double minor = 10.0 * kArcsec2Rad;
    double pa = 0.0;

    [[maybe_unused]] auto [maj_pix, min_pix, pa_pix] = gaussian_world_to_pixel(major, minor, pa, dc, {100.0, 100.0});

    // At Dec=45, the pixel scale in RA is cos(Dec) * cdelt.
    // So a 10 arcsec circular Gaussian in world should be elliptical in pixels
    // unless we account for the projection. With SIN projection near the center,
    // the pixel sizes should be roughly:
    // - along Dec axis: 10 pixels (10 arcsec / 1 arcsec/pixel)
    // - along RA axis: ~10/cos(45) pixels ≈ 14.1 pixels

    // The major axis in pixels should be around 10 pixels (matching minor in this case
    // since the Gaussian is circular, but pixel aspect ratio differs).
    // This is a rough check.
    assert(maj_pix > 5.0);
    assert(min_pix > 5.0);
    return true;
}

bool test_world_pixel_roundtrip() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           0.0, -kArcsec2Rad, kArcsec2Rad, {}, 100.0, 100.0);

    double major = 5.0 * kArcsec2Rad;
    double minor = 3.0 * kArcsec2Rad;
    double pa = 0.3; // radians

    std::vector<double> pixel = {100.0, 100.0};

    [[maybe_unused]] auto [maj_pix, min_pix, pa_pix] = gaussian_world_to_pixel(major, minor, pa, dc, pixel);

    auto [maj_back, min_back, pa_back] =
        gaussian_pixel_to_world(maj_pix, min_pix, pa_pix, dc, pixel);

    // Round-trip should recover original values within tolerance.
    assert(near(maj_back, major, 0.01));
    assert(near(min_back, minor, 0.01));

    // PA may differ by pi due to sign ambiguity.
    double pa_diff = std::abs(pa_back - pa);
    if (pa_diff > M_PI / 2) {
        pa_diff = std::abs(pa_diff - M_PI);
    }
    assert(pa_diff < 0.01);
    return true;
}

bool test_elongated_gaussian() {
    DirectionCoordinate dc(DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0,
                           0.0, -kArcsec2Rad, kArcsec2Rad, {}, 50.0, 50.0);

    double major = 10.0 * kArcsec2Rad;
    double minor = 2.0 * kArcsec2Rad;
    double pa = M_PI / 4.0; // 45 degrees

    [[maybe_unused]] auto [maj_pix, min_pix, pa_pix] = gaussian_world_to_pixel(major, minor, pa, dc, {50.0, 50.0});

    // Major should be much larger than minor in pixel space too.
    assert(maj_pix > min_pix * 2.0);
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

    std::cout << "gaussian_convert_test\n";
    run("circular_gaussian", test_circular_gaussian);
    run("world_pixel_roundtrip", test_world_pixel_roundtrip);
    run("elongated_gaussian", test_elongated_gaussian);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
