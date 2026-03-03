// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file image_malformed_test.cpp
/// Malformed-input hardening tests for PagedImage, SubImage, ImageExpr,
/// ImageConcat.  Verifies that invalid operations throw specific exceptions
/// rather than crashing or silently producing wrong results.

#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/image.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

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

#define EXPECT_THROW(expr, ExType)                                             \
    do {                                                                       \
        bool threw = false;                                                    \
        try {                                                                  \
            (void)(expr);                                                      \
        } catch (const ExType&) {                                              \
            threw = true;                                                      \
        }                                                                      \
        CHECK(threw);                                                          \
    } while (false)

// Helper: build a minimal 2D coord system.
static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {}},
        0.0, 0.0, -1e-4, 1e-4, std::vector<double>{1, 0, 0, 1}, 0.0, 0.0);
    cs.add_coordinate(std::move(dc));
    return cs;
}

// Create a writable tmp dir that is cleaned up at test end.
static std::filesystem::path tmp_dir() {
    auto p = std::filesystem::temp_directory_path() / "casacore_mini_img_malformed_test";
    std::filesystem::create_directories(p);
    return p;
}

// ── Read-only guard tests ─────────────────────────────────────────────

static void test_paged_image_readonly_put() {
    std::cerr << "  test_paged_image_readonly_put\n";
    auto dir = tmp_dir() / "ro_put.img";
    std::filesystem::remove_all(dir);
    {
        PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
        LatticeArray<float> data(IPosition{4, 4}, 1.0f);
        img.put(data);
    }
    // Re-open read-only.
    PagedImage<float> img(dir, /*writable=*/false);
    CHECK(!img.is_writable());

    LatticeArray<float> data(IPosition{4, 4}, 2.0f);
    EXPECT_THROW(img.put(data), std::runtime_error);
    EXPECT_THROW(img.put_at(5.0f, IPosition{0, 0}), std::runtime_error);

    LatticeArray<float> slice_data(IPosition{2, 2}, 3.0f);
    EXPECT_THROW(img.put_slice(slice_data, Slicer(IPosition{0, 0}, IPosition{2, 2})), std::runtime_error);
}

static void test_paged_image_readonly_mask() {
    std::cerr << "  test_paged_image_readonly_mask\n";
    auto dir = tmp_dir() / "ro_mask.img";
    std::filesystem::remove_all(dir);
    {
        PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
        LatticeArray<float> data(IPosition{4, 4}, 1.0f);
        img.put(data);
    }
    PagedImage<float> img(dir, /*writable=*/false);
    LatticeArray<bool> mask(IPosition{4, 4}, true);
    EXPECT_THROW(img.make_mask("mask0", mask), std::runtime_error);
}

// ── SubImage read-only guard ──────────────────────────────────────────

static void test_subimage_readonly_put() {
    std::cerr << "  test_subimage_readonly_put\n";
    auto dir = tmp_dir() / "sub_ro.img";
    std::filesystem::remove_all(dir);
    {
        PagedImage<float> img(IPosition{8, 8}, make_2d_cs(), dir);
        LatticeArray<float> data(IPosition{8, 8}, 1.0f);
        img.put(data);
    }
    PagedImage<float> img(dir, /*writable=*/false);

    Slicer sl(IPosition{0, 0}, IPosition{4, 4});
    SubImage<float> sub(img, sl);
    CHECK(!sub.is_writable());

    LatticeArray<float> data(IPosition{4, 4}, 2.0f);
    EXPECT_THROW(sub.put(data), std::runtime_error);
    EXPECT_THROW(sub.put_at(5.0f, IPosition{0, 0}), std::runtime_error);

    LatticeArray<float> slice_data(IPosition{2, 2}, 3.0f);
    EXPECT_THROW(sub.put_slice(slice_data, Slicer(IPosition{0, 0}, IPosition{2, 2})), std::runtime_error);
}

// ── SubImage slicer validation ────────────────────────────────────────

static void test_subimage_slicer_out_of_bounds() {
    std::cerr << "  test_subimage_slicer_out_of_bounds\n";
    auto dir = tmp_dir() / "sub_oob.img";
    std::filesystem::remove_all(dir);
    {
        PagedImage<float> img(IPosition{8, 8}, make_2d_cs(), dir);
        LatticeArray<float> data(IPosition{8, 8}, 1.0f);
        img.put(data);
    }
    PagedImage<float> img(dir, /*writable=*/false);

    // Slicer that extends beyond image shape.
    EXPECT_THROW((SubImage<float>(img, Slicer(IPosition{0, 0}, IPosition{10, 4}))),
                 std::out_of_range);
    EXPECT_THROW((SubImage<float>(img, Slicer(IPosition{6, 0}, IPosition{4, 4}))),
                 std::out_of_range);
}

// ── ImageExpr read-only guard ─────────────────────────────────────────

static void test_imageexpr_readonly() {
    std::cerr << "  test_imageexpr_readonly\n";
    auto dir = tmp_dir() / "expr_ro.img";
    std::filesystem::remove_all(dir);
    {
        PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
        LatticeArray<float> data(IPosition{4, 4}, 1.0f);
        img.put(data);
    }
    PagedImage<float> img(dir, /*writable=*/false);

    LatticeArray<float> edata(IPosition{4, 4}, 2.0f);
    ImageExpr<float> expr(std::move(edata), make_2d_cs());

    LatticeArray<float> data(IPosition{4, 4}, 2.0f);
    EXPECT_THROW(expr.put(data), std::runtime_error);
    EXPECT_THROW(expr.put_at(5.0f, IPosition{0, 0}), std::runtime_error);
}

// ── ImageConcat read-only guard ───────────────────────────────────────

static void test_imageconcat_readonly() {
    std::cerr << "  test_imageconcat_readonly\n";
    auto dir1 = tmp_dir() / "concat_ro_1.img";
    auto dir2 = tmp_dir() / "concat_ro_2.img";
    std::filesystem::remove_all(dir1);
    std::filesystem::remove_all(dir2);
    {
        PagedImage<float> img1(IPosition{4, 4}, make_2d_cs(), dir1);
        LatticeArray<float> d1(IPosition{4, 4}, 1.0f);
        img1.put(d1);
    }
    {
        PagedImage<float> img2(IPosition{4, 4}, make_2d_cs(), dir2);
        LatticeArray<float> d2(IPosition{4, 4}, 2.0f);
        img2.put(d2);
    }

    PagedImage<float> img1(dir1, false);
    PagedImage<float> img2(dir2, false);
    std::vector<const ImageInterface<float>*> imgs{&img1, &img2};
    ImageConcat<float> cat(imgs, 0);

    LatticeArray<float> data(cat.shape(), 3.0f);
    EXPECT_THROW(cat.put(data), std::runtime_error);
    EXPECT_THROW(cat.put_at(5.0f, IPosition{0, 0}), std::runtime_error);
}

// ── Open nonexistent path ─────────────────────────────────────────────

static void test_open_nonexistent() {
    std::cerr << "  test_open_nonexistent\n";
    auto dir = tmp_dir() / "does_not_exist_12345.img";
    std::filesystem::remove_all(dir);
    EXPECT_THROW(PagedImage<float>(dir, false), std::runtime_error);
}

// ── put_at out-of-bounds ──────────────────────────────────────────────

static void test_put_at_out_of_bounds() {
    std::cerr << "  test_put_at_out_of_bounds\n";
    auto dir = tmp_dir() / "put_oob.img";
    std::filesystem::remove_all(dir);
    PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
    LatticeArray<float> data(IPosition{4, 4}, 1.0f);
    img.put(data);

    EXPECT_THROW(img.put_at(5.0f, IPosition{10, 0}), std::out_of_range);
    EXPECT_THROW(img.put_at(5.0f, IPosition{0, 10}), std::out_of_range);
    EXPECT_THROW(img.put_at(5.0f, IPosition{-1, 0}), std::out_of_range);
}

// ── get_at out-of-bounds ──────────────────────────────────────────────

static void test_get_at_out_of_bounds() {
    std::cerr << "  test_get_at_out_of_bounds\n";
    auto dir = tmp_dir() / "get_oob.img";
    std::filesystem::remove_all(dir);
    PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
    LatticeArray<float> data(IPosition{4, 4}, 1.0f);
    img.put(data);

    EXPECT_THROW(img.get_at(IPosition{10, 0}), std::out_of_range);
    EXPECT_THROW(img.get_at(IPosition{0, 10}), std::out_of_range);
}

// ── put_slice beyond shape ────────────────────────────────────────────

static void test_put_slice_beyond_shape() {
    std::cerr << "  test_put_slice_beyond_shape\n";
    auto dir = tmp_dir() / "slice_oob.img";
    std::filesystem::remove_all(dir);
    PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
    LatticeArray<float> data(IPosition{4, 4}, 1.0f);
    img.put(data);

    LatticeArray<float> slice_data(IPosition{4, 4}, 2.0f);
    // Starting at (2,2) with size 4x4 overflows.
    EXPECT_THROW(img.put_slice(slice_data, Slicer(IPosition{2, 2}, IPosition{4, 4})), std::out_of_range);
}

// ── get_slice beyond shape ────────────────────────────────────────────

static void test_get_slice_beyond_shape() {
    std::cerr << "  test_get_slice_beyond_shape\n";
    auto dir = tmp_dir() / "gslice_oob.img";
    std::filesystem::remove_all(dir);
    PagedImage<float> img(IPosition{4, 4}, make_2d_cs(), dir);
    LatticeArray<float> data(IPosition{4, 4}, 1.0f);
    img.put(data);

    Slicer sl(IPosition{2, 2}, IPosition{4, 4});
    EXPECT_THROW(img.get_slice(sl), std::out_of_range);
}

// ── Main ──────────────────────────────────────────────────────────────

int main() {
    std::cerr << "image_malformed_test\n";

    test_paged_image_readonly_put();
    test_paged_image_readonly_mask();
    test_subimage_readonly_put();
    test_subimage_slicer_out_of_bounds();
    test_imageexpr_readonly();
    test_imageconcat_readonly();
    test_open_nonexistent();
    test_put_at_out_of_bounds();
    test_get_at_out_of_bounds();
    test_put_slice_beyond_shape();
    test_get_slice_beyond_shape();

    std::cerr << g_pass << " passed, " << g_fail << " failed\n";

    // Clean up.
    std::filesystem::remove_all(
        std::filesystem::temp_directory_path() / "casacore_mini_img_malformed_test");

    return g_fail != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
