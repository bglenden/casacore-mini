#include "casacore_mini/image.hpp"
#include "casacore_mini/linear_coordinate.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace casacore_mini;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__               \
                      << "]: " #cond << "\n";                                  \
            ++g_fail;                                                          \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                                  \
    do {                                                                       \
        if (std::abs(static_cast<double>(a) - static_cast<double>(b))          \
            < (tol)) {                                                         \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__               \
                      << "]: |" << (a) << " - " << (b)                         \
                      << "| >= " << (tol) << "\n";                             \
        }                                                                      \
    } while (false)

// ---------------------------------------------------------------------------
// Helper: build a 2D linear coordinate system with specified parameters.
// ---------------------------------------------------------------------------
static CoordinateSystem make_linear_cs_2d(double crval0, double crval1,
                                           double cdelt0, double cdelt1,
                                           double crpix0, double crpix1) {
    CoordinateSystem cs;
    LinearXform xform;
    xform.crval = {crval0, crval1};
    xform.cdelt = {cdelt0, cdelt1};
    xform.crpix = {crpix0, crpix1};
    cs.add_coordinate(std::make_unique<LinearCoordinate>(
        std::vector<std::string>{"x", "y"},
        std::vector<std::string>{"pix", "pix"},
        std::move(xform)));
    return cs;
}

// ---------------------------------------------------------------------------
// Test 1: Doubled resolution regrid.
//
// Source 8x8 with cdelt=(2,2): world = pixel * 2.
// Dst 16x16 with cdelt=(1,1): world = pixel * 1.
// Each dst pixel maps to src pixel = (dx/2, dy/2), rounded to nearest.
// ---------------------------------------------------------------------------
static void test_regrid_doubled_resolution() {
    auto src_cs = make_linear_cs_2d(0, 0, 2, 2, 0, 0);
    auto dst_cs = make_linear_cs_2d(0, 0, 1, 1, 0, 0);

    TempImage<float> src(IPosition{8, 8}, std::move(src_cs));
    TempImage<float> dst(IPosition{16, 16}, std::move(dst_cs));

    // Fill source: src[x,y] = y*8 + x + 1.
    for (std::int64_t y = 0; y < 8; ++y) {
        for (std::int64_t x = 0; x < 8; ++x) {
            src.put_at(static_cast<float>(y * 8 + x + 1), IPosition{x, y});
        }
    }

    image_regrid(src, dst);

    // Verify each dst pixel against nearest-neighbor expectation.
    for (std::int64_t dy = 0; dy < 16; ++dy) {
        for (std::int64_t dx = 0; dx < 16; ++dx) {
            // dst world = (dx, dy); src pixel = world / 2 = (dx/2, dy/2).
            auto sx = static_cast<std::int64_t>(std::round(static_cast<double>(dx) / 2.0));
            auto sy = static_cast<std::int64_t>(std::round(static_cast<double>(dy) / 2.0));

            float expected = 0.0F;
            if (sx >= 0 && sx < 8 && sy >= 0 && sy < 8) {
                expected = static_cast<float>(sy * 8 + sx + 1);
            }

            float actual = dst.get_at(IPosition{dx, dy});
            CHECK_NEAR(actual, expected, 1e-5);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 2: Offset crpix regrid.
//
// Source 8x8: crval=(0,0), cdelt=(1,1), crpix=(0,0) => world = pixel.
// Dst 8x8:   crval=(0,0), cdelt=(1,1), crpix=(2,2) => world = pixel - 2.
// Dst pixel (dx,dy) => world (dx-2, dy-2) => src pixel (dx-2, dy-2).
// In bounds only when dx-2 in [0,8) and dy-2 in [0,8), else zero.
// ---------------------------------------------------------------------------
static void test_regrid_offset_crpix() {
    auto src_cs = make_linear_cs_2d(0, 0, 1, 1, 0, 0);
    auto dst_cs = make_linear_cs_2d(0, 0, 1, 1, 2, 2);

    TempImage<float> src(IPosition{8, 8}, std::move(src_cs));
    TempImage<float> dst(IPosition{8, 8}, std::move(dst_cs));

    // Fill source: src[x,y] = y*8 + x + 1.
    for (std::int64_t y = 0; y < 8; ++y) {
        for (std::int64_t x = 0; x < 8; ++x) {
            src.put_at(static_cast<float>(y * 8 + x + 1), IPosition{x, y});
        }
    }

    image_regrid(src, dst);

    // Spot checks.
    // dst(2,2) => world (0,0) => src(0,0) = 1.
    CHECK_NEAR(dst.get_at(IPosition{2, 2}), 1.0F, 1e-5);
    // dst(3,3) => world (1,1) => src(1,1) = 1*8+1+1 = 10.
    CHECK_NEAR(dst.get_at(IPosition{3, 3}), 10.0F, 1e-5);
    // dst(0,0) => world (-2,-2) => src(-2,-2): out of bounds => 0.
    CHECK_NEAR(dst.get_at(IPosition{0, 0}), 0.0F, 1e-5);
    // dst(1,0) => world (-1,-2) => src(-1,-2): out of bounds => 0.
    CHECK_NEAR(dst.get_at(IPosition{1, 0}), 0.0F, 1e-5);

    // Full sweep: verify all pixels.
    for (std::int64_t dy = 0; dy < 8; ++dy) {
        for (std::int64_t dx = 0; dx < 8; ++dx) {
            auto sx = dx - 2;
            auto sy = dy - 2;

            float expected = 0.0F;
            if (sx >= 0 && sx < 8 && sy >= 0 && sy < 8) {
                expected = static_cast<float>(sy * 8 + sx + 1);
            }

            float actual = dst.get_at(IPosition{dx, dy});
            CHECK_NEAR(actual, expected, 1e-5);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 3: Out-of-bounds zero-fill.
//
// Source 4x4: crval=(0,0), cdelt=(1,1), crpix=(0,0).
// Dst 8x8:   crval=(0,0), cdelt=(1,1), crpix=(0,0) -- same world coords,
//            but dst is larger so pixels (4..7,*) and (*,4..7) have no
//            source data => zero-filled.
// ---------------------------------------------------------------------------
static void test_regrid_out_of_bounds_zero_fill() {
    auto src_cs = make_linear_cs_2d(0, 0, 1, 1, 0, 0);
    auto dst_cs = make_linear_cs_2d(0, 0, 1, 1, 0, 0);

    TempImage<float> src(IPosition{4, 4}, std::move(src_cs));
    TempImage<float> dst(IPosition{8, 8}, std::move(dst_cs));

    // Fill source: src[x,y] = y*4 + x + 1.
    for (std::int64_t y = 0; y < 4; ++y) {
        for (std::int64_t x = 0; x < 4; ++x) {
            src.put_at(static_cast<float>(y * 4 + x + 1), IPosition{x, y});
        }
    }

    image_regrid(src, dst);

    // In-bounds region: dst pixel == src pixel for (0..3, 0..3).
    for (std::int64_t dy = 0; dy < 4; ++dy) {
        for (std::int64_t dx = 0; dx < 4; ++dx) {
            float expected = static_cast<float>(dy * 4 + dx + 1);
            float actual = dst.get_at(IPosition{dx, dy});
            CHECK_NEAR(actual, expected, 1e-5);
        }
    }

    // Out-of-bounds region: should all be zero.
    for (std::int64_t dy = 0; dy < 8; ++dy) {
        for (std::int64_t dx = 0; dx < 8; ++dx) {
            if (dx >= 4 || dy >= 4) {
                float actual = dst.get_at(IPosition{dx, dy});
                CHECK_NEAR(actual, 0.0F, 1e-5);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() { // NOLINT(bugprone-exception-escape)
    std::cout << "image_regrid_test\n";

    test_regrid_doubled_resolution();
    test_regrid_offset_crpix();
    test_regrid_out_of_bounds_zero_fill();

    std::cout << g_pass << " checks passed, " << g_fail << " failed.\n";
    if (g_fail > 0) {
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
