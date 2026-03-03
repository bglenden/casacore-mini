// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/image.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/stokes_coordinate.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>

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

// Helper: create a minimal 2D coordinate system (direction).
static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {}},
        0.0, 0.0, -1e-5, 1e-5,
        std::vector<double>{1, 0, 0, 1}, 0.0, 0.0);
    cs.add_coordinate(std::move(dc));
    return cs;
}

// Helper: create a 4D coordinate system (direction + stokes + spectral).
static CoordinateSystem make_4d_cs() {
    CoordinateSystem cs;
    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {}},
        0.0, 0.0, -1e-5, 1e-5,
        std::vector<double>{1, 0, 0, 1}, 0.0, 0.0);
    cs.add_coordinate(std::move(dc));
    cs.add_coordinate(std::make_unique<StokesCoordinate>(
        std::vector<std::int32_t>{1})); // I only
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(
        FrequencyRef::lsrk, 1.4e9, 1e6, 0.0));
    return cs;
}

// ── ImageInfo tests ──────────────────────────────────────────────────

static void test_imageinfo_roundtrip() {
    ImageInfo info;
    info.beam_major_rad = 0.001;
    info.beam_minor_rad = 0.0005;
    info.beam_pa_rad = 1.2;
    info.object_name = "NGC 1234";
    info.image_type = "Intensity";

    CHECK(info.has_beam());

    auto rec = info.to_record();
    auto restored = ImageInfo::from_record(rec);
    CHECK(restored == info);
}

static void test_imageinfo_no_beam() {
    ImageInfo info;
    CHECK(!info.has_beam());
    info.object_name = "Test";
    auto rec = info.to_record();
    auto restored = ImageInfo::from_record(rec);
    CHECK(restored.object_name == "Test");
    CHECK(!restored.has_beam());
}

// ── TempImage tests ──────────────────────────────────────────────────

static void test_temp_image_basic() {
    TempImage<float> img(IPosition{4, 5}, make_2d_cs());

    CHECK(img.ndim() == 2);
    CHECK(img.nelements() == 20);
    CHECK(img.is_writable());
    CHECK(!img.is_persistent());
    CHECK(!img.has_pixel_mask());
    CHECK(img.name() == "TempImage");
}

static void test_temp_image_put_get() {
    TempImage<float> img(IPosition{3, 4}, make_2d_cs());

    img.put_at(42.0f, IPosition{1, 2});
    CHECK(img.get_at(IPosition{1, 2}) == 42.0f);
    CHECK(img.get_at(IPosition{0, 0}) == 0.0f);

    auto data = img.get();
    CHECK(data.nelements() == 12);
}

static void test_temp_image_slice() {
    TempImage<float> img(IPosition{4, 5}, make_2d_cs());
    img.put_at(7.0f, IPosition{1, 2});

    Slicer slicer(IPosition{0, 2}, IPosition{4, 1});
    auto slice = img.get_slice(slicer);
    CHECK(slice.nelements() == 4);
    CHECK(slice.at(IPosition{1, 0}) == 7.0f);
}

static void test_temp_image_put_whole() {
    TempImage<double> img(IPosition{3, 3}, make_2d_cs());

    LatticeArray<double> data(IPosition{3, 3}, 5.0);
    img.put(data);
    CHECK(img.get_at(IPosition{0, 0}) == 5.0);
    CHECK(img.get_at(IPosition{2, 2}) == 5.0);
}

static void test_temp_image_put_slice() {
    TempImage<float> img(IPosition{4, 4}, make_2d_cs());

    LatticeArray<float> patch(IPosition{2, 2}, 9.0f);
    Slicer slicer(IPosition{1, 1}, IPosition{2, 2});
    img.put_slice(patch, slicer);

    CHECK(img.get_at(IPosition{1, 1}) == 9.0f);
    CHECK(img.get_at(IPosition{2, 2}) == 9.0f);
    CHECK(img.get_at(IPosition{0, 0}) == 0.0f);
}

static void test_temp_image_metadata() {
    TempImage<float> img(IPosition{2, 2}, make_2d_cs());

    img.set_units("Jy/beam");
    CHECK(img.units() == "Jy/beam");

    ImageInfo info;
    info.beam_major_rad = 0.01;
    info.beam_minor_rad = 0.005;
    info.beam_pa_rad = 0.0;
    info.object_name = "Test Source";
    img.set_image_info(info);
    CHECK(img.image_info().object_name == "Test Source");
    CHECK(img.image_info().has_beam());

    Record misc;
    misc.set("author", RecordValue(std::string("tester")));
    img.set_misc_info(misc);
    CHECK(img.misc_info().size() == 1);
}

static void test_temp_image_coordinates() {
    auto cs = make_4d_cs();
    auto n = cs.n_pixel_axes();
    TempImage<float> img(IPosition{4, 4, 1, 8}, std::move(cs));
    CHECK(img.coordinates().n_pixel_axes() == n);

    img.set_coordinates(make_2d_cs());
    CHECK(img.coordinates().n_pixel_axes() == 2);
}

static void test_temp_image_mask() {
    TempImage<float> img(IPosition{3, 3}, make_2d_cs());

    CHECK(!img.has_pixel_mask());
    auto mask_all = img.get_mask();
    CHECK(mask_all.nelements() == 9);
    // All true when no mask attached.
    for (std::size_t i = 0; i < 9; ++i) {
        CHECK(mask_all.flat()[i] == true);
    }

    // Attach a mask.
    LatticeArray<bool> m(IPosition{3, 3}, true);
    m.make_unique();
    m.put(IPosition{1, 1}, false);
    img.attach_mask(std::move(m));

    CHECK(img.has_pixel_mask());
    auto mask = img.get_mask();
    CHECK(mask.at(IPosition{0, 0}) == true);
    CHECK(mask.at(IPosition{1, 1}) == false);

    Slicer slicer(IPosition{1, 1}, IPosition{2, 2});
    auto mask_slice = img.get_mask_slice(slicer);
    CHECK(mask_slice.at(IPosition{0, 0}) == false);
    CHECK(mask_slice.at(IPosition{1, 0}) == true);
}

// ── SubImage tests ───────────────────────────────────────────────────

static void test_sub_image_read() {
    TempImage<float> parent(IPosition{6, 8}, make_2d_cs());
    parent.put_at(42.0f, IPosition{2, 3});

    Slicer region(IPosition{1, 2}, IPosition{4, 4});
    SubImage<float> sub(parent, region);

    CHECK(sub.shape()[0] == 4);
    CHECK(sub.shape()[1] == 4);
    CHECK(sub.ndim() == 2);
    CHECK(sub.nelements() == 16);

    // (2,3) in parent = (1,1) in sub-image.
    CHECK(sub.get_at(IPosition{1, 1}) == 42.0f);
    CHECK(sub.get_at(IPosition{0, 0}) == 0.0f);
}

static void test_sub_image_write() {
    TempImage<float> parent(IPosition{6, 8}, make_2d_cs());

    Slicer region(IPosition{1, 2}, IPosition{4, 4});
    SubImage<float> sub(parent, region);

    CHECK(sub.is_writable());
    sub.put_at(99.0f, IPosition{0, 0});
    // (0,0) in sub = (1,2) in parent.
    CHECK(parent.get_at(IPosition{1, 2}) == 99.0f);
}

static void test_sub_image_readonly() {
    TempImage<float> parent(IPosition{4, 4}, make_2d_cs());

    const auto& cparent = parent;
    Slicer region(IPosition{0, 0}, IPosition{2, 2});
    SubImage<float> sub(cparent, region);

    CHECK(!sub.is_writable());
    // Writing should throw.
    bool threw = false;
    try {
        sub.put_at(1.0f, IPosition{0, 0});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_sub_image_get_whole() {
    TempImage<float> parent(IPosition{4, 5}, make_2d_cs());
    parent.put_at(3.0f, IPosition{1, 1});
    parent.put_at(7.0f, IPosition{2, 2});

    Slicer region(IPosition{1, 1}, IPosition{3, 3});
    SubImage<float> sub(parent, region);

    auto data = sub.get();
    CHECK(data.nelements() == 9);
    CHECK(data.at(IPosition{0, 0}) == 3.0f);
    CHECK(data.at(IPosition{1, 1}) == 7.0f);
}

static void test_sub_image_put_whole() {
    TempImage<float> parent(IPosition{4, 4}, make_2d_cs());

    Slicer region(IPosition{1, 1}, IPosition{2, 2});
    SubImage<float> sub(parent, region);

    LatticeArray<float> patch(IPosition{2, 2}, 5.0f);
    sub.put(patch);

    CHECK(parent.get_at(IPosition{1, 1}) == 5.0f);
    CHECK(parent.get_at(IPosition{2, 2}) == 5.0f);
    CHECK(parent.get_at(IPosition{0, 0}) == 0.0f);
}

static void test_sub_image_metadata_forwarding() {
    TempImage<float> parent(IPosition{4, 4}, make_2d_cs());
    parent.set_units("Jy/beam");

    ImageInfo info;
    info.object_name = "Parent Object";
    parent.set_image_info(info);

    Slicer region(IPosition{0, 0}, IPosition{2, 2});
    SubImage<float> sub(parent, region);

    // SubImage reads metadata from parent.
    CHECK(sub.units() == "Jy/beam");
    CHECK(sub.image_info().object_name == "Parent Object");
    CHECK(sub.name().find("(sub)") != std::string::npos);
}

static void test_sub_image_get_slice() {
    TempImage<float> parent(IPosition{8, 8}, make_2d_cs());
    parent.put_at(11.0f, IPosition{3, 3});

    Slicer region(IPosition{2, 2}, IPosition{4, 4});
    SubImage<float> sub(parent, region);

    // (3,3) parent = (1,1) sub
    Slicer local_slice(IPosition{0, 0}, IPosition{2, 2});
    auto sliced = sub.get_slice(local_slice);
    CHECK(sliced.nelements() == 4);
    CHECK(sliced.at(IPosition{1, 1}) == 11.0f);
}

static void test_sub_image_put_slice() {
    TempImage<float> parent(IPosition{6, 6}, make_2d_cs());

    Slicer region(IPosition{1, 1}, IPosition{4, 4});
    SubImage<float> sub(parent, region);

    LatticeArray<float> patch(IPosition{2, 2}, 33.0f);
    Slicer local_slice(IPosition{1, 1}, IPosition{2, 2});
    sub.put_slice(patch, local_slice);

    // region start=(1,1), local_slice start=(1,1): parent=(2,2).
    CHECK(parent.get_at(IPosition{2, 2}) == 33.0f);
    CHECK(parent.get_at(IPosition{3, 3}) == 33.0f);
    CHECK(parent.get_at(IPosition{0, 0}) == 0.0f);
}

static void test_sub_image_mask() {
    TempImage<float> parent(IPosition{4, 4}, make_2d_cs());

    // No mask on parent -> sub returns all-true.
    Slicer region(IPosition{1, 1}, IPosition{2, 2});
    SubImage<float> sub(parent, region);

    auto mask = sub.get_mask();
    CHECK(mask.nelements() == 4);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(mask.flat()[i] == true);
    }

    // Attach mask to parent, then create a new sub.
    LatticeArray<bool> m(IPosition{4, 4}, true);
    m.make_unique();
    m.put(IPosition{2, 2}, false);
    parent.attach_mask(std::move(m));

    SubImage<float> sub2(parent, region);
    auto mask2 = sub2.get_mask();
    // (2,2) parent = (1,1) in sub.
    CHECK(mask2.at(IPosition{1, 1}) == false);
    CHECK(mask2.at(IPosition{0, 0}) == true);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    // ImageInfo
    test_imageinfo_roundtrip();
    test_imageinfo_no_beam();

    // TempImage
    test_temp_image_basic();
    test_temp_image_put_get();
    test_temp_image_slice();
    test_temp_image_put_whole();
    test_temp_image_put_slice();
    test_temp_image_metadata();
    test_temp_image_coordinates();
    test_temp_image_mask();

    // SubImage
    test_sub_image_read();
    test_sub_image_write();
    test_sub_image_readonly();
    test_sub_image_get_whole();
    test_sub_image_put_whole();
    test_sub_image_metadata_forwarding();
    test_sub_image_get_slice();
    test_sub_image_put_slice();
    test_sub_image_mask();

    std::cout << "image_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
