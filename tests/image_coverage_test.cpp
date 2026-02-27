/// @file
/// @brief Comprehensive coverage tests for the image class hierarchy.
///
/// Tests PagedImage, TempImage, SubImage, ImageExpr, ImageConcat,
/// ImageBeamSet, ImageSummary, and image_utilities::statistics.

#include "casacore_mini/image.hpp"
#include "casacore_mini/lattice.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/linear_coordinate.hpp"

#include <cmath>
#include <complex>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace casacore_mini;

// ── Test infrastructure ────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ \
                      << "]: " #cond << "\n"; \
            ++g_fail; \
        } else { \
            ++g_pass; \
        } \
    } while (false)

#define CHECK_NEAR(a, b, tol) \
    do { \
        if (std::abs(static_cast<double>(a) - static_cast<double>(b)) \
            < (tol)) { \
            ++g_pass; \
        } else { \
            ++g_fail; \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ \
                      << "]: |" << (a) << " - " << (b) \
                      << "| >= " << (tol) << "\n"; \
        } \
    } while (false)

#define CHECK_THROWS(expr) \
    do { \
        bool caught = false; \
        try { (void)(expr); } catch (...) { caught = true; } \
        if (caught) { ++g_pass; } else { \
            ++g_fail; \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ \
                      << "]: expected throw from " << #expr << "\n"; \
        } \
    } while (false)

// ── Helper functions ───────────────────────────────────────────────────

static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {}},
        0.0, 0.0, -1e-5, 1e-5,
        std::vector<double>{1, 0, 0, 1}, 0.0, 0.0));
    return cs;
}

static std::filesystem::path make_temp_dir(const std::string& prefix) {
    auto dir = std::filesystem::temp_directory_path() /
               ("casacore_mini_test_" + prefix + "_" +
                std::to_string(reinterpret_cast<std::uintptr_t>(&prefix)));
    std::filesystem::create_directories(dir);
    return dir;
}

// ═══════════════════════════════════════════════════════════════════════
// P1 — PagedImage mask roundtrip
// ═══════════════════════════════════════════════════════════════════════

static void test_paged_image_mask_roundtrip() {
    std::cout << "  paged image mask roundtrip... ";
    auto temp_dir = make_temp_dir("mask_rt");
    auto img_path = temp_dir / "mask_test.img";

    // Create and populate.
    constexpr std::int64_t kN = 32;
    {
        PagedImage<float> img(IPosition{kN, kN}, make_2d_cs(), img_path);
        LatticeArray<float> data(IPosition{kN, kN});
        data.make_unique();
        for (std::int64_t y = 0; y < kN; ++y) {
            for (std::int64_t x = 0; x < kN; ++x) {
                data.put(IPosition{x, y},
                         static_cast<float>(y * kN + x));
            }
        }
        img.put(data);

        // Build mask: true except where (x+y)%3 == 0.
        const auto npix = static_cast<std::size_t>(kN * kN);
        std::vector<bool> mask_vec(npix);
        for (std::int64_t y = 0; y < kN; ++y) {
            for (std::int64_t x = 0; x < kN; ++x) {
                mask_vec[static_cast<std::size_t>(y * kN + x)] =
                    ((x + y) % 3 != 0);
            }
        }
        LatticeArray<bool> mask_data(IPosition{kN, kN}, std::move(mask_vec));
        img.make_mask("mask0", mask_data, true);
        img.flush();
    }

    // Reopen read-only and verify.
    {
        PagedImage<float> img(img_path, false);
        CHECK(img.has_pixel_mask() == true);

        auto mask = img.get_mask();
        CHECK(mask.shape() == IPosition({kN, kN}));

        for (std::int64_t y = 0; y < kN; ++y) {
            for (std::int64_t x = 0; x < kN; ++x) {
                bool expected = ((x + y) % 3 != 0);
                CHECK(mask.at(IPosition{x, y}) == expected);
            }
        }

        // Test get_mask_slice: extract a 4x4 sub-region starting at (2,2).
        auto mask_slice = img.get_mask_slice(
            Slicer(IPosition{2, 2}, IPosition{4, 4}));
        CHECK(mask_slice.shape() == IPosition({4, 4}));
        for (std::int64_t dy = 0; dy < 4; ++dy) {
            for (std::int64_t dx = 0; dx < 4; ++dx) {
                bool expected = (((dx + 2) + (dy + 2)) % 3 != 0);
                CHECK(mask_slice.at(IPosition{dx, dy}) == expected);
            }
        }
    }

    std::filesystem::remove_all(temp_dir);
    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P2 — PagedImage metadata roundtrip
// ═══════════════════════════════════════════════════════════════════════

static void test_paged_image_metadata_roundtrip() {
    std::cout << "  paged image metadata roundtrip... ";
    auto temp_dir = make_temp_dir("meta_rt");
    auto img_path = temp_dir / "meta_test.img";

    {
        PagedImage<float> img(IPosition{32, 32}, make_2d_cs(), img_path);
        img.set_units("Jy/beam");

        ImageInfo info;
        info.beam_major_rad = 1e-4;
        info.beam_minor_rad = 5e-5;
        info.beam_pa_rad = 0.1;
        img.set_image_info(info);

        Record misc;
        misc.set("author", RecordValue(std::string("test")));
        img.set_misc_info(misc);

        img.flush();
    }

    {
        PagedImage<float> img(img_path, false);

        CHECK(img.coordinates().n_coordinates() >= 1);

        // Check reference value (crval) of first coordinate.
        auto crval = img.coordinates().coordinate(0).reference_value();
        CHECK_NEAR(crval[0], 0.0, 1e-12);
        CHECK_NEAR(crval[1], 0.0, 1e-12);

        // Check increment (cdelt) of first coordinate.
        auto cdelt = img.coordinates().coordinate(0).increment();
        CHECK_NEAR(cdelt[0], -1e-5, 1e-12);
        CHECK_NEAR(cdelt[1], 1e-5, 1e-12);

        // Check reference pixel (crpix) of first coordinate.
        auto crpix = img.coordinates().coordinate(0).reference_pixel();
        CHECK_NEAR(crpix[0], 0.0, 1e-12);
        CHECK_NEAR(crpix[1], 0.0, 1e-12);

        CHECK(img.units() == "Jy/beam");
        CHECK(img.image_info().has_beam());
        CHECK(img.misc_info().find("author") != nullptr);

        // name() should contain the path.
        auto nm = img.name();
        CHECK(nm.find("meta_test.img") != std::string::npos);
    }

    std::filesystem::remove_all(temp_dir);
    std::cout << "ok\n";
}

static void test_paged_image_name_returns_path() {
    std::cout << "  paged image name returns path... ";
    auto temp_dir = make_temp_dir("name_rt");
    auto img_path = temp_dir / "name_check.img";

    PagedImage<float> img(IPosition{32, 32}, make_2d_cs(), img_path);
    auto nm = img.name();
    CHECK(nm.find("name_check.img") != std::string::npos);

    img.flush();
    std::filesystem::remove_all(temp_dir);
    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P3 — PagedImage<complex<float>> roundtrip
// ═══════════════════════════════════════════════════════════════════════

static void test_paged_image_complex_roundtrip() {
    std::cout << "  paged image complex roundtrip... ";
    auto temp_dir = make_temp_dir("complex_rt");
    auto img_path = temp_dir / "complex_test.img";

    constexpr std::int64_t kCN = 32;
    {
        PagedImage<std::complex<float>> img(
            IPosition{kCN, kCN}, make_2d_cs(), img_path);
        LatticeArray<std::complex<float>> data(IPosition{kCN, kCN});
        data.make_unique();
        for (std::int64_t y = 0; y < kCN; ++y) {
            for (std::int64_t x = 0; x < kCN; ++x) {
                data.put(IPosition{x, y},
                         std::complex<float>(
                             static_cast<float>(x + 1),
                             static_cast<float>(y + 1)));
            }
        }
        img.put(data);
        img.flush();
    }

    {
        PagedImage<std::complex<float>> img(img_path, false);
        for (std::int64_t y = 0; y < kCN; ++y) {
            for (std::int64_t x = 0; x < kCN; ++x) {
                auto val = img.get_at(IPosition{x, y});
                CHECK_NEAR(val.real(), static_cast<float>(x + 1), 1e-6);
                CHECK_NEAR(val.imag(), static_cast<float>(y + 1), 1e-6);
            }
        }
    }

    std::filesystem::remove_all(temp_dir);
    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P4 — SubImage metadata and read-only enforcement
// ═══════════════════════════════════════════════════════════════════════

static void test_sub_image_set_units_roundtrip() {
    std::cout << "  sub image set_units roundtrip... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());
    parent.set_units("Jy/beam");

    SubImage<float> sub(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    sub.set_units("K");
    CHECK(parent.units() == "K");
    CHECK(sub.units() == "K");
    std::cout << "ok\n";
}

static void test_sub_image_set_image_info() {
    std::cout << "  sub image set_image_info... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());

    SubImage<float> sub(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    ImageInfo info;
    info.beam_major_rad = 2e-4;
    info.beam_minor_rad = 1e-4;
    info.beam_pa_rad = 0.5;
    sub.set_image_info(info);

    CHECK(parent.image_info().has_beam());
    CHECK_NEAR(parent.image_info().beam_major_rad, 2e-4, 1e-12);
    std::cout << "ok\n";
}

static void test_sub_image_set_misc_info() {
    std::cout << "  sub image set_misc_info... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());

    SubImage<float> sub(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    Record misc;
    misc.set("key1", RecordValue(std::string("val1")));
    sub.set_misc_info(misc);

    CHECK(parent.misc_info().find("key1") != nullptr);
    std::cout << "ok\n";
}

static void test_sub_image_coordinates() {
    std::cout << "  sub image coordinates... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());

    SubImage<float> sub(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    CHECK(sub.coordinates().n_coordinates() >= 1);
    CHECK(sub.coordinates().n_pixel_axes() == 2);
    std::cout << "ok\n";
}

static void test_sub_image_is_persistent() {
    std::cout << "  sub image is_persistent... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());

    SubImage<float> sub(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    CHECK(sub.is_persistent() == false);
    std::cout << "ok\n";
}

static void test_sub_image_readonly_throws_on_put() {
    std::cout << "  sub image readonly throws on put... ";
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());
    parent.set(1.0F);

    // Create read-only SubImage by passing a const reference.
    const TempImage<float>& const_parent = parent;
    SubImage<float> sub(const_parent,
                        Slicer(IPosition{0, 0}, IPosition{4, 4}));
    CHECK(sub.is_writable() == false);

    CHECK_THROWS(sub.put_at(5.0F, IPosition{0, 0}));

    LatticeArray<float> slice_data(IPosition{2, 2}, 3.0F);
    CHECK_THROWS(sub.put_slice(slice_data,
                               Slicer(IPosition{0, 0}, IPosition{2, 2})));

    LatticeArray<float> full_data(IPosition{4, 4}, 3.0F);
    CHECK_THROWS(sub.put(full_data));

    CHECK_THROWS(sub.set_units("x"));
    CHECK_THROWS(sub.set_image_info(ImageInfo{}));
    CHECK_THROWS(sub.set_misc_info(Record{}));
    std::cout << "ok\n";
}

static void test_sub_image_has_pixel_mask() {
    std::cout << "  sub image has_pixel_mask... ";

    // No mask on parent.
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());
    SubImage<float> sub1(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    CHECK(sub1.has_pixel_mask() == false);

    // Attach mask to parent.
    LatticeArray<bool> mask(IPosition{8, 8}, true);
    parent.attach_mask(mask);
    SubImage<float> sub2(parent, Slicer(IPosition{0, 0}, IPosition{4, 4}));
    CHECK(sub2.has_pixel_mask() == true);

    std::cout << "ok\n";
}

static void test_sub_image_get_mask_slice() {
    std::cout << "  sub image get_mask_slice... ";

    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());

    // Create a mask with a pattern.
    std::vector<bool> mask_vec(64);
    for (std::size_t i = 0; i < 64; ++i) {
        mask_vec[i] = (i % 2 == 0);
    }
    LatticeArray<bool> mask(IPosition{8, 8}, std::move(mask_vec));
    parent.attach_mask(mask);

    // SubImage covering rows [2..5], cols [2..5].
    SubImage<float> sub(parent, Slicer(IPosition{2, 2}, IPosition{4, 4}));

    // Get the full sub mask.
    auto sub_mask = sub.get_mask();
    CHECK(sub_mask.shape() == IPosition({4, 4}));

    // Verify element-by-element against parent mask.
    for (std::int64_t dy = 0; dy < 4; ++dy) {
        for (std::int64_t dx = 0; dx < 4; ++dx) {
            bool expected = mask.at(IPosition{dx + 2, dy + 2});
            CHECK(sub_mask.at(IPosition{dx, dy}) == expected);
        }
    }

    // Also test get_mask_slice within the sub-image.
    auto sub_mask_slice = sub.get_mask_slice(
        Slicer(IPosition{1, 1}, IPosition{2, 2}));
    CHECK(sub_mask_slice.shape() == IPosition({2, 2}));
    for (std::int64_t dy = 0; dy < 2; ++dy) {
        for (std::int64_t dx = 0; dx < 2; ++dx) {
            bool expected = mask.at(IPosition{dx + 3, dy + 3});
            CHECK(sub_mask_slice.at(IPosition{dx, dy}) == expected);
        }
    }

    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P5 — ImageExpr read-only verification
// ═══════════════════════════════════════════════════════════════════════

static void test_image_expr_get() {
    std::cout << "  image expr get... ";

    // Build a 3x3 data array.
    std::vector<float> raw(9);
    for (std::size_t i = 0; i < 9; ++i) {
        raw[i] = static_cast<float>(i + 1);
    }
    LatticeArray<float> data(IPosition{3, 3}, std::move(raw));

    ImageExpr<float> expr(data, make_2d_cs(), "test_expr");

    // Verify get() returns correct data.
    auto full = expr.get();
    CHECK(full.shape() == IPosition({3, 3}));
    for (std::size_t i = 0; i < 9; ++i) {
        CHECK_NEAR(full[i], static_cast<float>(i + 1), 1e-6);
    }

    // Verify get_at.
    CHECK_NEAR(expr.get_at(IPosition{0, 0}), 1.0F, 1e-6);
    CHECK_NEAR(expr.get_at(IPosition{2, 2}), 9.0F, 1e-6);

    // Verify get_slice.
    auto slice = expr.get_slice(Slicer(IPosition{1, 1}, IPosition{2, 2}));
    CHECK(slice.shape() == IPosition({2, 2}));
    // Elements at (1,1)=5, (2,1)=6, (1,2)=8, (2,2)=9 in 3x3 Fortran order.
    CHECK_NEAR(slice.at(IPosition{0, 0}), 5.0F, 1e-6);
    CHECK_NEAR(slice.at(IPosition{1, 0}), 6.0F, 1e-6);
    CHECK_NEAR(slice.at(IPosition{0, 1}), 8.0F, 1e-6);
    CHECK_NEAR(slice.at(IPosition{1, 1}), 9.0F, 1e-6);

    std::cout << "ok\n";
}

static void test_image_expr_read_only() {
    std::cout << "  image expr read only... ";
    LatticeArray<float> data(IPosition{3, 3}, 1.0F);
    ImageExpr<float> expr(data, make_2d_cs(), "ro_test");

    CHECK(expr.is_writable() == false);

    LatticeArray<float> slice_data(IPosition{2, 2}, 5.0F);
    CHECK_THROWS(expr.put_slice(slice_data,
                                Slicer(IPosition{0, 0}, IPosition{2, 2})));
    CHECK_THROWS(expr.put_at(1.0F, IPosition{0, 0}));

    LatticeArray<float> full_data(IPosition{3, 3}, 2.0F);
    CHECK_THROWS(expr.put(full_data));

    std::cout << "ok\n";
}

static void test_image_expr_metadata() {
    std::cout << "  image expr metadata... ";
    LatticeArray<float> data(IPosition{3, 3}, 1.0F);
    ImageExpr<float> expr(data, make_2d_cs(), "metadata_test");

    expr.set_units("Jy/beam");
    CHECK(expr.units() == "Jy/beam");

    Record misc;
    misc.set("key", RecordValue(std::string("value")));
    expr.set_misc_info(misc);
    CHECK(expr.misc_info().find("key") != nullptr);

    CHECK(expr.coordinates().n_coordinates() >= 1);
    CHECK(expr.expression() == "metadata_test");
    CHECK(expr.name().find("ImageExpr") != std::string::npos);

    std::cout << "ok\n";
}

static void test_image_expr_mask() {
    std::cout << "  image expr mask... ";
    LatticeArray<float> data(IPosition{3, 3}, 1.0F);
    ImageExpr<float> expr(data, make_2d_cs(), "mask_test");

    CHECK(expr.has_pixel_mask() == false);

    auto mask = expr.get_mask();
    CHECK(mask.shape() == IPosition({3, 3}));
    for (std::size_t i = 0; i < mask.nelements(); ++i) {
        CHECK(mask[i] == true);
    }

    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P6 — ImageConcat verification
// ═══════════════════════════════════════════════════════════════════════

static void test_image_concat_basic() {
    std::cout << "  image concat basic... ";

    TempImage<float> img1(IPosition{4, 3}, make_2d_cs());
    img1.set(1.0F);

    TempImage<float> img2(IPosition{4, 3}, make_2d_cs());
    img2.set(2.0F);

    std::vector<const ImageInterface<float>*> images = {&img1, &img2};
    ImageConcat<float> concat(images, 0);

    CHECK(concat.shape() == IPosition({8, 3}));

    auto full = concat.get();
    CHECK(full.shape() == IPosition({8, 3}));

    CHECK_NEAR(concat.get_at(IPosition{0, 0}), 1.0F, 1e-6);
    CHECK_NEAR(concat.get_at(IPosition{3, 2}), 1.0F, 1e-6);
    CHECK_NEAR(concat.get_at(IPosition{4, 0}), 2.0F, 1e-6);
    CHECK_NEAR(concat.get_at(IPosition{7, 2}), 2.0F, 1e-6);

    // Verify get_slice.
    auto slice = concat.get_slice(
        Slicer(IPosition{3, 0}, IPosition{2, 2}));
    CHECK(slice.shape() == IPosition({2, 2}));
    CHECK_NEAR(slice.at(IPosition{0, 0}), 1.0F, 1e-6);
    CHECK_NEAR(slice.at(IPosition{1, 0}), 2.0F, 1e-6);

    std::cout << "ok\n";
}

static void test_image_concat_read_only() {
    std::cout << "  image concat read only... ";

    TempImage<float> img1(IPosition{4, 3}, make_2d_cs());
    TempImage<float> img2(IPosition{4, 3}, make_2d_cs());

    std::vector<const ImageInterface<float>*> images = {&img1, &img2};
    ImageConcat<float> concat(images, 0);

    CHECK(concat.is_writable() == false);

    LatticeArray<float> slice_data(IPosition{2, 2}, 5.0F);
    CHECK_THROWS(concat.put_slice(
        slice_data, Slicer(IPosition{0, 0}, IPosition{2, 2})));
    CHECK_THROWS(concat.put_at(1.0F, IPosition{0, 0}));

    LatticeArray<float> full_data(IPosition{8, 3}, 2.0F);
    CHECK_THROWS(concat.put(full_data));

    std::cout << "ok\n";
}

static void test_image_concat_metadata() {
    std::cout << "  image concat metadata... ";

    TempImage<float> img1(IPosition{4, 3}, make_2d_cs());
    img1.set_units("Jy/beam");

    TempImage<float> img2(IPosition{4, 3}, make_2d_cs());
    img2.set_units("K");

    std::vector<const ImageInterface<float>*> images = {&img1, &img2};
    ImageConcat<float> concat(images, 0);

    // Units should come from first image.
    CHECK(concat.units() == "Jy/beam");
    CHECK(concat.coordinates().n_coordinates() >= 1);
    CHECK(concat.name().find("ImageConcat") != std::string::npos);

    std::cout << "ok\n";
}

static void test_image_concat_mask() {
    std::cout << "  image concat mask... ";

    TempImage<float> img1(IPosition{4, 3}, make_2d_cs());
    TempImage<float> img2(IPosition{4, 3}, make_2d_cs());

    std::vector<const ImageInterface<float>*> images = {&img1, &img2};
    ImageConcat<float> concat(images, 0);

    CHECK(concat.has_pixel_mask() == false);

    auto mask = concat.get_mask();
    CHECK(mask.shape() == IPosition({8, 3}));
    for (std::size_t i = 0; i < mask.nelements(); ++i) {
        CHECK(mask[i] == true);
    }

    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P7 — ImageBeamSet edge cases
// ═══════════════════════════════════════════════════════════════════════

static void test_image_beamset_default_empty() {
    std::cout << "  image beamset default empty... ";
    ImageBeamSet bs;
    CHECK(bs.nchan() == 0);
    CHECK(bs.nstokes() == 0);
    std::cout << "ok\n";
}

static void test_image_beamset_single() {
    std::cout << "  image beamset single... ";
    ImageBeamSet::Beam beam{1e-4, 5e-5, 0.1};
    ImageBeamSet bs(beam);
    CHECK(bs.is_single());
    CHECK(bs.nchan() == 1);
    CHECK(bs.nstokes() == 1);

    auto& got = bs.get(0, 0);
    CHECK_NEAR(got.major, 1e-4, 1e-12);
    CHECK_NEAR(got.minor, 5e-5, 1e-12);
    CHECK_NEAR(got.pa, 0.1, 1e-12);
    std::cout << "ok\n";
}

static void test_image_beamset_grid() {
    std::cout << "  image beamset grid... ";
    ImageBeamSet bs(4, 2);
    CHECK(bs.nchan() == 4);
    CHECK(bs.nstokes() == 2);

    // Set beams at various positions.
    bs.set(0, 0, {1e-4, 5e-5, 0.0});
    bs.set(1, 0, {2e-4, 1e-4, 0.1});
    bs.set(3, 1, {3e-4, 1.5e-4, 0.2});

    CHECK_NEAR(bs.get(0, 0).major, 1e-4, 1e-12);
    CHECK_NEAR(bs.get(1, 0).major, 2e-4, 1e-12);
    CHECK_NEAR(bs.get(3, 1).major, 3e-4, 1e-12);

    // Unset beams should be zero.
    CHECK_NEAR(bs.get(2, 0).major, 0.0, 1e-12);
    CHECK_NEAR(bs.get(0, 1).major, 0.0, 1e-12);

    std::cout << "ok\n";
}

static void test_image_beamset_roundtrip() {
    std::cout << "  image beamset roundtrip... ";
    ImageBeamSet bs(2, 1);
    bs.set(0, 0, {1e-4, 5e-5, 0.0});
    bs.set(1, 0, {2e-4, 1e-4, 0.1});

    Record rec = bs.to_record();
    auto restored = ImageBeamSet::from_record(rec);

    CHECK(restored.nchan() == 2);
    CHECK(restored.nstokes() == 1);
    CHECK_NEAR(restored.get(0, 0).major, 1e-4, 1e-12);
    CHECK_NEAR(restored.get(1, 0).major, 2e-4, 1e-12);
    CHECK_NEAR(restored.get(1, 0).pa, 0.1, 1e-12);

    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P8 — ImageSummary edge cases
// ═══════════════════════════════════════════════════════════════════════

static void test_image_summary_no_beam() {
    std::cout << "  image summary no beam... ";
    TempImage<float> img(IPosition{4, 4}, make_2d_cs());

    std::ostringstream oss;
    ImageSummary::print(img, oss);
    auto text = oss.str();

    CHECK(text.find("Image:") != std::string::npos);
    CHECK(text.find("Beam:") == std::string::npos);
    std::cout << "ok\n";
}

static void test_image_summary_with_beam() {
    std::cout << "  image summary with beam... ";
    TempImage<float> img(IPosition{4, 4}, make_2d_cs());

    ImageInfo info;
    info.beam_major_rad = 1e-4;
    info.beam_minor_rad = 5e-5;
    info.beam_pa_rad = 0.0;
    img.set_image_info(info);

    std::ostringstream oss;
    ImageSummary::print(img, oss);
    auto text = oss.str();

    CHECK(text.find("Beam:") != std::string::npos);
    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// P9 — image_utilities::statistics edge cases
// ═══════════════════════════════════════════════════════════════════════

static void test_statistics_basic() {
    std::cout << "  statistics basic... ";
    TempImage<float> img(IPosition{4, 4}, make_2d_cs());

    // Fill with values 0..15.
    for (std::int64_t y = 0; y < 4; ++y) {
        for (std::int64_t x = 0; x < 4; ++x) {
            float val = static_cast<float>(y * 4 + x);
            img.put_at(val, IPosition{x, y});
        }
    }

    auto stats = image_utilities::statistics(img);
    CHECK(stats.npixels == 16);
    CHECK_NEAR(stats.min_val, 0.0F, 1e-6);
    CHECK_NEAR(stats.max_val, 15.0F, 1e-6);

    // Mean of 0..15 = 7.5.
    CHECK_NEAR(stats.mean, 7.5, 1e-6);

    // RMS = sqrt(mean of squares) = sqrt((0^2+1^2+...+15^2)/16).
    // Sum of squares: 0+1+4+9+16+25+36+49+64+81+100+121+144+169+196+225 = 1240
    // RMS = sqrt(1240/16) = sqrt(77.5)
    double expected_rms = std::sqrt(77.5);
    CHECK_NEAR(stats.rms, expected_rms, 1e-4);

    std::cout << "ok\n";
}

static void test_statistics_empty_image() {
    std::cout << "  statistics empty image... ";
    // 0-element image: statistics should return npixels==0 and default values.
    TempImage<float> img(IPosition{0}, CoordinateSystem{});
    auto stats = image_utilities::statistics(img);
    CHECK(stats.npixels == 0);
    CHECK_NEAR(stats.mean, 0.0, 1e-12);
    CHECK_NEAR(stats.rms, 0.0, 1e-12);
    std::cout << "ok\n";
}

// ═══════════════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "image_coverage_test:\n";

    // P1 — PagedImage mask roundtrip
    test_paged_image_mask_roundtrip();

    // P2 — PagedImage metadata roundtrip
    test_paged_image_metadata_roundtrip();
    test_paged_image_name_returns_path();

    // P3 — PagedImage<complex<float>> roundtrip
    test_paged_image_complex_roundtrip();

    // P4 — SubImage metadata and read-only enforcement
    test_sub_image_set_units_roundtrip();
    test_sub_image_set_image_info();
    test_sub_image_set_misc_info();
    test_sub_image_coordinates();
    test_sub_image_is_persistent();
    test_sub_image_readonly_throws_on_put();
    test_sub_image_has_pixel_mask();
    test_sub_image_get_mask_slice();

    // P5 — ImageExpr read-only verification
    test_image_expr_get();
    test_image_expr_read_only();
    test_image_expr_metadata();
    test_image_expr_mask();

    // P6 — ImageConcat verification
    test_image_concat_basic();
    test_image_concat_read_only();
    test_image_concat_metadata();
    test_image_concat_mask();

    // P7 — ImageBeamSet edge cases
    test_image_beamset_default_empty();
    test_image_beamset_single();
    test_image_beamset_grid();
    test_image_beamset_roundtrip();

    // P8 — ImageSummary edge cases
    test_image_summary_no_beam();
    test_image_summary_with_beam();

    // P9 — image_utilities::statistics
    test_statistics_basic();
    test_statistics_empty_image();

    std::cout << "\n  passed: " << g_pass << "  failed: " << g_fail << "\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
