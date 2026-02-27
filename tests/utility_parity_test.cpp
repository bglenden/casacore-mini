/// @file utility_parity_test.cpp
/// @brief P10-W9 tests: utility-layer parity and integration cleanup.

#include "casacore_mini/image.hpp"
#include "casacore_mini/lattice.hpp"
#include "casacore_mini/lattice_expr.hpp"
#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/linear_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace casacore_mini;

// ── Helpers ──────────────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(expr)                                                    \
    do {                                                               \
        if (expr) {                                                    \
            ++g_pass;                                                  \
        } else {                                                       \
            ++g_fail;                                                  \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__       \
                      << "]: " << #expr << "\n";                       \
        }                                                              \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                          \
    do {                                                               \
        if (std::abs(static_cast<double>(a) - static_cast<double>(b))  \
            < (tol)) {                                                 \
            ++g_pass;                                                  \
        } else {                                                       \
            ++g_fail;                                                  \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__       \
                      << "]: |" << (a) << " - " << (b)                 \
                      << "| >= " << (tol) << "\n";                     \
        }                                                              \
    } while (false)

#define CHECK_THROWS(expr)                                             \
    do {                                                               \
        bool caught = false;                                           \
        try { (void)(expr); } catch (...) { caught = true; }           \
        if (caught) { ++g_pass; } else {                               \
            ++g_fail;                                                  \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__       \
                      << "]: expected throw from " << #expr << "\n";   \
        }                                                              \
    } while (false)

static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    // DirectionCoordinate(ref, proj, ref_lon_rad, ref_lat_rad,
    //                     inc_lon_rad, inc_lat_rad, pc, crpix_x, crpix_y)
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000,
        Projection{ProjectionType::sin, {}},
        0.0, 0.0,       // reference lon/lat in radians
        -1e-4, 1e-4,    // increments in radians
        std::vector<double>{1.0, 0.0, 0.0, 1.0}, // identity PC matrix
        5.0, 5.0));      // reference pixel (0-based)
    return cs;
}

// ── LatticeConcat tests ──────────────────────────────────────────────

static void test_lattice_concat() {
    // Two 4x3 lattices concatenated along axis 0 → 8x3.
    ArrayLattice<float> a(IPosition{4, 3}, 1.0f);
    ArrayLattice<float> b(IPosition{4, 3}, 2.0f);

    std::vector<const Lattice<float>*> lats = {&a, &b};
    LatticeConcat<float> cat(lats, 0);

    CHECK(cat.shape()[0] == 8);
    CHECK(cat.shape()[1] == 3);
    CHECK(cat.nlattices() == 2);
    CHECK(cat.axis() == 0);

    // Values from first lattice.
    CHECK_NEAR(cat.get_at(IPosition{0, 0}), 1.0f, 1e-6);
    CHECK_NEAR(cat.get_at(IPosition{3, 2}), 1.0f, 1e-6);
    // Values from second lattice.
    CHECK_NEAR(cat.get_at(IPosition{4, 0}), 2.0f, 1e-6);
    CHECK_NEAR(cat.get_at(IPosition{7, 2}), 2.0f, 1e-6);

    // Full read.
    auto full = cat.get();
    CHECK(full.shape()[0] == 8);
    CHECK_NEAR(full.at(IPosition{0, 0}), 1.0f, 1e-6);
    CHECK_NEAR(full.at(IPosition{5, 1}), 2.0f, 1e-6);

    // Read-only.
    CHECK(!cat.is_writable());
    CHECK_THROWS(cat.put_at(0.0f, IPosition{0, 0}));
}

static void test_lattice_concat_axis1() {
    // Concat along axis 1: two 3x4 → 3x8.
    ArrayLattice<float> a(IPosition{3, 4}, 10.0f);
    ArrayLattice<float> b(IPosition{3, 4}, 20.0f);

    std::vector<const Lattice<float>*> lats = {&a, &b};
    LatticeConcat<float> cat(lats, 1);

    CHECK(cat.shape()[0] == 3);
    CHECK(cat.shape()[1] == 8);
    CHECK_NEAR(cat.get_at(IPosition{0, 3}), 10.0f, 1e-6);
    CHECK_NEAR(cat.get_at(IPosition{0, 4}), 20.0f, 1e-6);
}

static void test_lattice_concat_validation() {
    ArrayLattice<float> a(IPosition{4, 3});
    ArrayLattice<float> b(IPosition{5, 4}); // mismatch on axis 1

    std::vector<const Lattice<float>*> lats = {&a, &b};
    CHECK_THROWS(LatticeConcat<float>(lats, 0));

    // Empty list.
    std::vector<const Lattice<float>*> empty;
    CHECK_THROWS(LatticeConcat<float>(empty, 0));
}

// ── RebinLattice tests ───────────────────────────────────────────────

static void test_rebin_lattice() {
    // 8x8 lattice with values 1..64, rebin by 2x2 → 4x4.
    ArrayLattice<float> src(IPosition{8, 8});
    {
        auto data = src.get();
        data.make_unique();
        for (std::int64_t i = 0; i < 64; ++i) {
            data.mutable_data()[i] = static_cast<float>(i + 1);
        }
        src.put(data);
    }

    RebinLattice<float> rb(src, IPosition{2, 2});
    CHECK(rb.shape()[0] == 4);
    CHECK(rb.shape()[1] == 4);

    // Top-left bin: avg of src[0,0], src[1,0], src[0,1], src[1,1].
    // In column-major: [0,0]=1, [1,0]=2, [0,1]=9, [1,1]=10 → avg = 5.5
    CHECK_NEAR(rb.get_at(IPosition{0, 0}), 5.5f, 1e-4);

    // Full read.
    auto full = rb.get();
    CHECK(full.shape()[0] == 4);

    // Read-only.
    CHECK(!rb.is_writable());
    CHECK_THROWS(rb.put_at(0.0f, IPosition{0, 0}));
}

static void test_rebin_non_divisible() {
    // 7x5 with factor 3x2 → ceil(7/3)=3, ceil(5/2)=3 → 3x3.
    ArrayLattice<float> src(IPosition{7, 5}, 6.0f);
    RebinLattice<float> rb(src, IPosition{3, 2});
    CHECK(rb.shape()[0] == 3);
    CHECK(rb.shape()[1] == 3);
    // All values are 6.0, so all rebinned values should be 6.0.
    CHECK_NEAR(rb.get_at(IPosition{0, 0}), 6.0f, 1e-6);
    CHECK_NEAR(rb.get_at(IPosition{2, 2}), 6.0f, 1e-6);
}

// ── ImageExpr tests ──────────────────────────────────────────────────

static void test_image_expr() {
    // Create an ImageExpr from evaluated data.
    LatticeArray<float> data(IPosition{4, 4}, 42.0f);
    ImageExpr<float> expr(data, make_2d_cs(), "42.0");

    CHECK(expr.shape()[0] == 4);
    CHECK(expr.shape()[1] == 4);
    CHECK_NEAR(expr.get_at(IPosition{0, 0}), 42.0f, 1e-6);
    CHECK(expr.name() == "ImageExpr(42.0)");
    CHECK(expr.expression() == "42.0");
    CHECK(!expr.is_writable());
    CHECK_THROWS(expr.put_at(0.0f, IPosition{0, 0}));

    // Metadata.
    expr.set_units("Jy/beam");
    CHECK(expr.units() == "Jy/beam");

    ImageInfo info;
    info.beam_major_rad = 0.01;
    info.beam_minor_rad = 0.005;
    info.beam_pa_rad = 0.1;
    expr.set_image_info(info);
    CHECK(expr.image_info().has_beam());
}

// ── ImageConcat tests ────────────────────────────────────────────────

static void test_image_concat() {
    TempImage<float> a(IPosition{4, 3}, make_2d_cs());
    a.put(LatticeArray<float>(IPosition{4, 3}, 10.0f));
    a.set_units("Jy");

    TempImage<float> b(IPosition{4, 3}, make_2d_cs());
    b.put(LatticeArray<float>(IPosition{4, 3}, 20.0f));

    std::vector<const ImageInterface<float>*> imgs = {&a, &b};
    ImageConcat<float> cat(imgs, 0);

    CHECK(cat.shape()[0] == 8);
    CHECK(cat.shape()[1] == 3);
    CHECK(cat.nimages() == 2);
    CHECK_NEAR(cat.get_at(IPosition{0, 0}), 10.0f, 1e-6);
    CHECK_NEAR(cat.get_at(IPosition{5, 1}), 20.0f, 1e-6);
    CHECK(cat.units() == "Jy");
    CHECK(!cat.is_writable());
    CHECK_THROWS(cat.put_at(0.0f, IPosition{0, 0}));
}

// ── ImageBeamSet tests ───────────────────────────────────────────────

static void test_image_beam_set() {
    // Single-beam constructor.
    ImageBeamSet::Beam single{0.01, 0.005, 0.1};
    ImageBeamSet bs(single);
    CHECK(bs.is_single());
    CHECK(bs.nchan() == 1);
    CHECK(bs.nstokes() == 1);
    CHECK_NEAR(bs.get(0, 0).major, 0.01, 1e-10);
    CHECK_NEAR(bs.get(0, 0).minor, 0.005, 1e-10);

    // Multi-beam constructor.
    ImageBeamSet mbs(4, 2);
    CHECK(!mbs.is_single());
    CHECK(mbs.nchan() == 4);
    CHECK(mbs.nstokes() == 2);

    mbs.set(2, 1, {0.02, 0.01, 0.3});
    CHECK_NEAR(mbs.get(2, 1).major, 0.02, 1e-10);

    // Serialization roundtrip.
    auto rec = mbs.to_record();
    auto restored = ImageBeamSet::from_record(rec);
    CHECK(restored.nchan() == 4);
    CHECK(restored.nstokes() == 2);
    CHECK_NEAR(restored.get(2, 1).major, 0.02, 1e-10);
    CHECK_NEAR(restored.get(2, 1).pa, 0.3, 1e-10);
}

// ── MaskSpecifier tests ──────────────────────────────────────────────

static void test_mask_specifier() {
    MaskSpecifier def;
    CHECK(def.use_default());
    CHECK(def.name.empty());

    MaskSpecifier named("mask0");
    CHECK(!named.use_default());
    CHECK(named.name == "mask0");
}

// ── ImageSummary tests ───────────────────────────────────────────────

static void test_image_summary() {
    TempImage<float> img(IPosition{8, 8}, make_2d_cs());
    img.set_units("Jy/beam");
    ImageInfo info;
    info.beam_major_rad = 0.01;
    info.beam_minor_rad = 0.005;
    info.beam_pa_rad = 0.1;
    info.object_name = "NGC 1234";
    img.set_image_info(info);

    std::ostringstream oss;
    ImageSummary::print(img, oss);
    auto text = oss.str();

    CHECK(text.find("TempImage") != std::string::npos);
    CHECK(text.find("8, 8") != std::string::npos);
    CHECK(text.find("Jy/beam") != std::string::npos);
    CHECK(text.find("Beam:") != std::string::npos);
    CHECK(text.find("NGC 1234") != std::string::npos);
}

// ── image_utilities tests ────────────────────────────────────────────

static void test_image_statistics() {
    TempImage<float> img(IPosition{4, 4}, make_2d_cs());
    LatticeArray<float> data(IPosition{4, 4});
    for (std::int64_t i = 0; i < 16; ++i) {
        data.mutable_data()[i] = static_cast<float>(i + 1);
    }
    img.put(data);

    auto stats = image_utilities::statistics(img);
    CHECK(stats.npixels == 16);
    CHECK_NEAR(stats.min_val, 1.0f, 1e-6);
    CHECK_NEAR(stats.max_val, 16.0f, 1e-6);
    CHECK_NEAR(stats.mean, 8.5, 1e-6);
    // rms = sqrt(sum(i^2)/n) for i=1..16
    double sum_sq = 0;
    for (int i = 1; i <= 16; ++i) sum_sq += i * i;
    CHECK_NEAR(stats.rms, std::sqrt(sum_sq / 16.0), 1e-4);
}

static void test_image_copy() {
    TempImage<float> src(IPosition{4, 4}, make_2d_cs());
    src.put(LatticeArray<float>(IPosition{4, 4}, 7.0f));
    src.set_units("K");
    ImageInfo info;
    info.object_name = "M31";
    src.set_image_info(info);

    TempImage<float> dst(IPosition{4, 4}, CoordinateSystem());
    image_utilities::copy_image(src, dst);

    CHECK_NEAR(dst.get_at(IPosition{0, 0}), 7.0f, 1e-6);
    CHECK(dst.units() == "K");
    CHECK(dst.image_info().object_name == "M31");
}

static void test_image_scale() {
    TempImage<float> src(IPosition{4, 4}, make_2d_cs());
    src.put(LatticeArray<float>(IPosition{4, 4}, 10.0f));

    TempImage<float> dst(IPosition{4, 4}, make_2d_cs());
    image_utilities::scale_image(src, dst, 2.0, 5.0);

    // 10 * 2 + 5 = 25
    CHECK_NEAR(dst.get_at(IPosition{0, 0}), 25.0f, 1e-6);
    CHECK_NEAR(dst.get_at(IPosition{3, 3}), 25.0f, 1e-6);
}

// ── CoordinateSystem to_world/to_pixel tests ─────────────────────────

static void test_cs_world_pixel_roundtrip() {
    auto cs = make_2d_cs();

    // Reference pixel should map to reference value.
    auto world = cs.to_world({5.0, 5.0});
    CHECK(world.size() == 2);
    CHECK_NEAR(world[0], 0.0, 1e-8);
    CHECK_NEAR(world[1], 0.0, 1e-8);

    // Roundtrip: pixel → world → pixel.
    auto pixel = cs.to_pixel(world);
    CHECK(pixel.size() == 2);
    CHECK_NEAR(pixel[0], 5.0, 1e-6);
    CHECK_NEAR(pixel[1], 5.0, 1e-6);

    // Off-reference pixel.
    auto world2 = cs.to_world({6.0, 7.0});
    auto pixel2 = cs.to_pixel(world2);
    CHECK_NEAR(pixel2[0], 6.0, 1e-4);
    CHECK_NEAR(pixel2[1], 7.0, 1e-4);
}

// ── image_regrid test ────────────────────────────────────────────────

static void test_image_regrid_identity() {
    // Regrid from src to dst with identical coordinate systems → copy.
    TempImage<float> src(IPosition{8, 8}, make_2d_cs());
    LatticeArray<float> data(IPosition{8, 8});
    for (std::int64_t i = 0; i < 64; ++i) {
        data.mutable_data()[i] = static_cast<float>(i + 1);
    }
    src.put(data);

    TempImage<float> dst(IPosition{8, 8}, make_2d_cs());
    image_regrid(src, dst);

    // With identical CS, output should match input.
    auto dst_data = dst.get();
    for (std::size_t i = 0; i < 64; ++i) {
        CHECK_NEAR(dst_data.flat()[i], static_cast<float>(i + 1), 1e-4);
    }
}

// ── main ─────────────────────────────────────────────────────────────

int main() {
    test_lattice_concat();
    test_lattice_concat_axis1();
    test_lattice_concat_validation();
    test_rebin_lattice();
    test_rebin_non_divisible();
    test_image_expr();
    test_image_concat();
    test_image_beam_set();
    test_mask_specifier();
    test_image_summary();
    test_image_statistics();
    test_image_copy();
    test_image_scale();
    test_cs_world_pixel_roundtrip();
    test_image_regrid_identity();

    std::cout << "utility_parity_test: " << g_pass << " passed, "
              << g_fail << " failed\n";
    if (g_fail > 0) {
        std::cerr << "\nutility_parity_test: " << g_pass << " passed, "
                  << g_fail << " failed\n";
    }
    return g_fail > 0 ? 1 : 0;
}
