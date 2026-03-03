// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/image.hpp"
#include "casacore_mini/lattice.hpp"
#include "casacore_mini/lattice_expr.hpp"
#include "casacore_mini/direction_coordinate.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
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
    CHECK(std::abs(static_cast<double>(a) - static_cast<double>(b)) < (tol))

static fs::path test_dir() {
    return fs::path(CASACORE_MINI_SOURCE_DIR) / "build-p10" / "test_mutation";
}

static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    DirectionCoordinate dc(
        DirectionRef::j2000,
        Projection{ProjectionType::sin, {}},
        0.0, 0.0,         // ref lon, lat
        -1.0e-3, 1.0e-3,  // inc lon, lat
        {1.0, 0.0, 0.0, 1.0}, // PC
        50.0, 50.0);       // crpix
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(std::move(dc)));
    return cs;
}

// ── PagedArray full lifecycle ────────────────────────────────────────

static void test_paged_array_roundtrip() {
    auto dir = test_dir() / "paged_array_rt";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{8, 8};

    // Create and write.
    {
        PagedArray<float> pa(sh, dir);
        LatticeArray<float> data(sh, 0.0f);
        data.make_unique();
        for (std::int64_t j = 0; j < 8; ++j) {
            for (std::int64_t i = 0; i < 8; ++i) {
                data.put(IPosition{i, j},
                         static_cast<float>(i + j * 8 + 1));
            }
        }
        pa.put(data);
        pa.flush();
    }

    // Re-open and verify.
    {
        PagedArray<float> pa(dir);
        CHECK(pa.shape() == sh);
        auto data = pa.get();
        CHECK_NEAR(data.flat()[0], 1.0f, 1e-6);
        CHECK_NEAR(data.flat()[63], 64.0f, 1e-6);
    }

    fs::remove_all(dir);
}

static void test_paged_array_put_at() {
    auto dir = test_dir() / "paged_array_putat";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{4, 4};

    {
        PagedArray<float> pa(sh, dir);
        LatticeArray<float> data(sh, 0.0f);
        pa.put(data);
        pa.put_at(42.0f, IPosition{2, 3});
        pa.flush();
    }
    {
        PagedArray<float> pa(dir);
        CHECK_NEAR(pa.get_at(IPosition{2, 3}), 42.0f, 1e-6);
        CHECK_NEAR(pa.get_at(IPosition{0, 0}), 0.0f, 1e-6);
    }

    fs::remove_all(dir);
}

static void test_paged_array_put_slice() {
    auto dir = test_dir() / "paged_array_slice";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{8, 8};

    {
        PagedArray<float> pa(sh, dir);
        LatticeArray<float> zeros(sh, 0.0f);
        pa.put(zeros);

        // Write a 4x4 slice at offset (2,2).
        LatticeArray<float> patch(IPosition{4, 4}, 99.0f);
        pa.put_slice(patch, Slicer(IPosition{2, 2}, IPosition{4, 4}));
        pa.flush();
    }
    {
        PagedArray<float> pa(dir);
        CHECK_NEAR(pa.get_at(IPosition{0, 0}), 0.0f, 1e-6);
        CHECK_NEAR(pa.get_at(IPosition{3, 3}), 99.0f, 1e-6);
        CHECK_NEAR(pa.get_at(IPosition{5, 5}), 99.0f, 1e-6);
    }

    fs::remove_all(dir);
}

static void test_paged_array_double_roundtrip() {
    auto dir = test_dir() / "paged_array_double";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{4, 4};
    {
        PagedArray<double> pa(sh, dir);
        LatticeArray<double> data(sh, 0.0);
        data.make_unique();
        data.put(IPosition{0, 0}, 3.141592653589793);
        data.put(IPosition{3, 3}, 2.718281828459045);
        pa.put(data);
        pa.flush();
    }
    {
        PagedArray<double> pa(dir);
        CHECK_NEAR(pa.get_at(IPosition{0, 0}), 3.141592653589793, 1e-12);
        CHECK_NEAR(pa.get_at(IPosition{3, 3}), 2.718281828459045, 1e-12);
    }

    fs::remove_all(dir);
}

// ── PagedImage full lifecycle ────────────────────────────────────────

static void test_paged_image_roundtrip() {
    auto dir = test_dir() / "paged_image_rt";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{100, 100};

    {
        PagedImage<float> img(sh, make_2d_cs(), dir);
        LatticeArray<float> data(sh, 0.0f);
        data.make_unique();
        for (std::int64_t j = 0; j < 100; ++j) {
            for (std::int64_t i = 0; i < 100; ++i) {
                data.put(IPosition{i, j},
                         static_cast<float>(i * 100 + j));
            }
        }
        img.put(data);
        img.set_units("Jy/beam");
        ImageInfo info;
        info.beam_major_rad = 1e-4;
        info.beam_minor_rad = 5e-5;
        info.beam_pa_rad = 0.5;
        info.object_name = "NGC 1234";
        info.image_type = "Intensity";
        img.set_image_info(info);
        Record misc;
        misc.set("author", RecordValue(std::string("test")));
        img.set_misc_info(misc);
        img.flush();
    }

    // Re-open and verify everything.
    {
        PagedImage<float> img(dir);
        CHECK(img.shape() == sh);
        CHECK(!img.is_writable()); // opened read-only by default
        CHECK(img.is_persistent());

        // Pixel data.
        CHECK_NEAR(img.get_at(IPosition{0, 0}), 0.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{50, 50}), 5050.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{99, 99}), 9999.0f, 1e-6);

        // Metadata.
        CHECK(img.units() == "Jy/beam");
        CHECK(img.image_info().has_beam());
        CHECK_NEAR(img.image_info().beam_major_rad, 1e-4, 1e-10);
        CHECK_NEAR(img.image_info().beam_minor_rad, 5e-5, 1e-10);
        CHECK_NEAR(img.image_info().beam_pa_rad, 0.5, 1e-10);
        CHECK(img.image_info().object_name == "NGC 1234");
        CHECK(img.image_info().image_type == "Intensity");

        // Coordinate system.
        CHECK(img.coordinates().n_coordinates() == 1);
    }

    fs::remove_all(dir);
}

static void test_paged_image_metadata_update() {
    auto dir = test_dir() / "paged_image_meta";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{10, 10};

    {
        PagedImage<float> img(sh, make_2d_cs(), dir);
        LatticeArray<float> data(sh, 1.0f);
        img.put(data);
        img.set_units("K");
        img.flush();
    }

    // Re-open, update metadata, flush, re-open again.
    {
        PagedImage<float> img(dir, true);
        CHECK(img.units() == "K");
        img.set_units("mJy/beam");
        ImageInfo info;
        info.beam_major_rad = 2e-4;
        info.beam_minor_rad = 1e-4;
        info.beam_pa_rad = 0.0;
        img.set_image_info(info);
        img.flush();
    }

    {
        PagedImage<float> img(dir);
        CHECK(img.units() == "mJy/beam");
        CHECK_NEAR(img.image_info().beam_major_rad, 2e-4, 1e-10);
    }

    fs::remove_all(dir);
}

static void test_paged_image_pixel_update() {
    auto dir = test_dir() / "paged_image_update";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{10, 10};

    {
        PagedImage<float> img(sh, make_2d_cs(), dir);
        LatticeArray<float> data(sh, 0.0f);
        img.put(data);
        img.flush();
    }

    // Re-open writable, modify some pixels, flush, re-open.
    {
        PagedImage<float> img(dir, true);
        img.put_at(123.0f, IPosition{5, 5});
        img.put_at(456.0f, IPosition{9, 9});
        img.flush();
    }

    {
        PagedImage<float> img(dir);
        CHECK_NEAR(img.get_at(IPosition{0, 0}), 0.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{5, 5}), 123.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{9, 9}), 456.0f, 1e-6);
    }

    fs::remove_all(dir);
}

static void test_paged_image_slice_update() {
    auto dir = test_dir() / "paged_image_slice_upd";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{20, 20};

    {
        PagedImage<float> img(sh, make_2d_cs(), dir);
        LatticeArray<float> data(sh, 1.0f);
        img.put(data);
        img.flush();
    }
    {
        PagedImage<float> img(dir, true);
        LatticeArray<float> patch(IPosition{5, 5}, -1.0f);
        img.put_slice(patch, Slicer(IPosition{10, 10}, IPosition{5, 5}));
        img.flush();
    }
    {
        PagedImage<float> img(dir);
        CHECK_NEAR(img.get_at(IPosition{0, 0}), 1.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{12, 12}), -1.0f, 1e-6);
    }

    fs::remove_all(dir);
}

// ── SubImage write-through ──────────────────────────────────────────

static void test_subimage_write_persists() {
    auto dir = test_dir() / "subimage_write";
    if (fs::exists(dir)) fs::remove_all(dir);

    IPosition sh{20, 20};

    {
        PagedImage<float> img(sh, make_2d_cs(), dir);
        LatticeArray<float> data(sh, 0.0f);
        img.put(data);

        // Write through SubImage.
        SubImage<float> sub(img, Slicer(IPosition{5, 5}, IPosition{10, 10}));
        sub.put_at(77.0f, IPosition{0, 0}); // → parent (5,5)
        sub.put_at(88.0f, IPosition{9, 9}); // → parent (14,14)
        img.flush();
    }

    {
        PagedImage<float> img(dir);
        CHECK_NEAR(img.get_at(IPosition{5, 5}), 77.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{14, 14}), 88.0f, 1e-6);
        CHECK_NEAR(img.get_at(IPosition{0, 0}), 0.0f, 1e-6);
    }

    fs::remove_all(dir);
}

// ── TempImage mask mutation ─────────────────────────────────────────

static void test_temp_image_mask_mutation() {
    IPosition sh{4, 4};
    TempImage<float> img(sh, make_2d_cs());

    LatticeArray<float> data(sh, 5.0f);
    img.put(data);

    // Initially no mask.
    CHECK(!img.has_pixel_mask());

    // Attach mask.
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    mask.put(IPosition{0, 0}, false);
    mask.put(IPosition{1, 1}, false);
    img.attach_mask(mask);
    CHECK(img.has_pixel_mask());

    auto m = img.get_mask();
    CHECK(m.flat()[0] == false);
    CHECK(m.flat()[5] == false); // (1,1) → linearized index
    CHECK(m.flat()[2] == true);
}

// ── Copy-on-write correctness ───────────────────────────────────────

static void test_copy_on_write_isolation() {
    IPosition sh{4, 4};
    LatticeArray<float> a(sh, 1.0f);
    LatticeArray<float> b = a; // shared

    b.make_unique();
    b.put(IPosition{0, 0}, 99.0f);

    // a should be untouched.
    CHECK_NEAR(a.flat()[0], 1.0f, 1e-6);
    CHECK_NEAR(b.flat()[0], 99.0f, 1e-6);
}

// ── Read-only error tests ───────────────────────────────────────────

static void test_read_only_sublattice_error() {
    ArrayLattice<float> lat(IPosition{4, 4}, 1.0f);
    const ArrayLattice<float>& clat = lat;
    SubLattice<float> sub(clat, Slicer(IPosition{0, 0}, IPosition{2, 2}));
    CHECK(!sub.is_writable());

    bool threw = false;
    try {
        sub.put_at(5.0f, IPosition{0, 0});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

static void test_read_only_subimage_error() {
    IPosition sh{10, 10};
    TempImage<float> img(sh, make_2d_cs());
    LatticeArray<float> data(sh, 1.0f);
    img.put(data);

    const TempImage<float>& cimg = img;
    SubImage<float> sub(cimg, Slicer(IPosition{0, 0}, IPosition{5, 5}));
    CHECK(!sub.is_writable());

    bool threw = false;
    try {
        sub.put_at(5.0f, IPosition{0, 0});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

// ── Expression result write-back ─────────────────────────────────────

static void test_expr_result_writeback() {
    IPosition sh{8, 8};
    TempImage<float> img(sh, make_2d_cs());

    // Create data: 1..64.
    LatticeArray<float> data(sh, 0.0f);
    data.make_unique();
    for (std::int64_t j = 0; j < 8; ++j) {
        for (std::int64_t i = 0; i < 8; ++i) {
            data.put(IPosition{i, j}, static_cast<float>(i + j * 8 + 1));
        }
    }
    img.put(data);

    // Evaluate expression: sqrt(data) → write result back.
    auto node = lel_math1(LelFunc::sqrt_f, lel_array(data));
    auto result = node->eval();
    img.put(result.values);

    CHECK_NEAR(img.get_at(IPosition{0, 0}), 1.0f, 1e-5);  // sqrt(1)
    CHECK_NEAR(img.get_at(IPosition{3, 0}), 2.0f, 1e-5);  // sqrt(4)
    CHECK_NEAR(img.get_at(IPosition{7, 7}), 8.0f, 1e-5);  // sqrt(64)
}

// ── ArrayLattice set/apply mutation ──────────────────────────────────

static void test_lattice_set_and_apply() {
    ArrayLattice<float> lat(IPosition{4, 4});
    lat.set(5.0f);
    CHECK_NEAR(lat.get_at(IPosition{2, 2}), 5.0f, 1e-6);

    lat.apply([](float v) { return v * 2.0f; });
    CHECK_NEAR(lat.get_at(IPosition{2, 2}), 10.0f, 1e-6);
}

// ── Iterator write on ArrayLattice ──────────────────────────────────

static void test_iterator_write_cursor() {
    ArrayLattice<float> lat(IPosition{8, 8});
    lat.set(0.0f);

    LatticeIterator<float> it(lat, IPosition{4, 4});
    // First cursor (0,0)→(3,3): fill with 1.0.
    LatticeArray<float> chunk(IPosition{4, 4}, 1.0f);
    it.write_cursor(chunk);
    ++it;
    // Second cursor (4,0)→(7,3): fill with 2.0.
    LatticeArray<float> chunk2(IPosition{4, 4}, 2.0f);
    it.write_cursor(chunk2);

    CHECK_NEAR(lat.get_at(IPosition{0, 0}), 1.0f, 1e-6);
    CHECK_NEAR(lat.get_at(IPosition{4, 0}), 2.0f, 1e-6);
    CHECK_NEAR(lat.get_at(IPosition{0, 4}), 0.0f, 1e-6); // not yet written
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    auto base = test_dir();
    if (fs::exists(base)) fs::remove_all(base);
    fs::create_directories(base);

    // PagedArray persistence.
    test_paged_array_roundtrip();
    test_paged_array_put_at();
    test_paged_array_put_slice();
    test_paged_array_double_roundtrip();

    // PagedImage persistence.
    test_paged_image_roundtrip();
    test_paged_image_metadata_update();
    test_paged_image_pixel_update();
    test_paged_image_slice_update();

    // SubImage write-through.
    test_subimage_write_persists();

    // Mask mutation.
    test_temp_image_mask_mutation();

    // Copy-on-write.
    test_copy_on_write_isolation();

    // Read-only errors.
    test_read_only_sublattice_error();
    test_read_only_subimage_error();

    // Expression write-back.
    test_expr_result_writeback();

    // Lattice set/apply.
    test_lattice_set_and_apply();

    // Iterator write.
    test_iterator_write_cursor();

    // Cleanup.
    if (fs::exists(base)) fs::remove_all(base);

    std::cout << "mutation_persist_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
