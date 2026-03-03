// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/lattice_region.hpp"

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

// ── LcBox tests ──────────────────────────────────────────────────────

static void test_lcbox_basic() {
    IPosition lat_shape{10, 10};
    LcBox box(IPosition{2, 3}, IPosition{5, 7}, lat_shape);

    CHECK(box.type() == "LcBox");
    CHECK(box.ndim() == 2);
    CHECK(box.blc()[0] == 2);
    CHECK(box.blc()[1] == 3);
    CHECK(box.trc()[0] == 5);
    CHECK(box.trc()[1] == 7);
    CHECK(box.shape()[0] == 4); // 5-2+1
    CHECK(box.shape()[1] == 5); // 7-3+1
    CHECK(!box.has_mask()); // LcBox has no mask
}

static void test_lcbox_from_slicer() {
    IPosition lat_shape{20, 20};
    Slicer s(IPosition{1, 2}, IPosition{5, 6});
    LcBox box(s, lat_shape);

    CHECK(box.blc()[0] == 1);
    CHECK(box.blc()[1] == 2);
    CHECK(box.trc()[0] == 5); // 1+5-1
    CHECK(box.trc()[1] == 7); // 2+6-1
    CHECK(box.shape()[0] == 5);
    CHECK(box.shape()[1] == 6);
}

static void test_lcbox_mask() {
    IPosition lat_shape{10, 10};
    LcBox box(IPosition{0, 0}, IPosition{2, 2}, lat_shape);
    auto mask = box.get_mask();
    CHECK(mask.nelements() == 9); // 3x3
    // All true (no mask).
    for (std::size_t i = 0; i < 9; ++i) {
        CHECK(mask.flat()[i] == true);
    }
}

static void test_lcbox_roundtrip() {
    IPosition lat_shape{10, 10};
    LcBox box(IPosition{1, 2}, IPosition{4, 6}, lat_shape);
    auto rec = box.to_record();
    auto restored = LcBox::from_record(rec, lat_shape);
    CHECK(restored->blc()[0] == 1);
    CHECK(restored->blc()[1] == 2);
    CHECK(restored->trc()[0] == 4);
    CHECK(restored->trc()[1] == 6);
}

static void test_lcbox_clone() {
    IPosition lat_shape{10, 10};
    LcBox box(IPosition{1, 1}, IPosition{3, 3}, lat_shape);
    auto cloned = box.clone();
    CHECK(cloned->type() == "LcBox");
    CHECK(cloned->shape()[0] == 3);
}

// ── LcPixelSet tests ─────────────────────────────────────────────────

static void test_lcpixelset_basic() {
    IPosition lat_shape{10, 10};
    LatticeArray<bool> mask(IPosition{3, 3}, false);
    mask.make_unique();
    mask.put(IPosition{1, 1}, true);
    mask.put(IPosition{0, 0}, true);

    LcPixelSet ps(std::move(mask), IPosition{2, 2}, lat_shape);
    CHECK(ps.type() == "LcPixelSet");
    CHECK(ps.has_mask());
    CHECK(ps.shape()[0] == 3);
    CHECK(ps.shape()[1] == 3);

    auto m = ps.get_mask();
    CHECK(m.at(IPosition{1, 1}) == true);
    CHECK(m.at(IPosition{0, 0}) == true);
    CHECK(m.at(IPosition{2, 2}) == false);
}

// ── LcEllipsoid tests ────────────────────────────────────────────────

static void test_lcellipsoid_basic() {
    IPosition lat_shape{20, 20};
    LcEllipsoid ell({10.0, 10.0}, {3.0, 5.0}, lat_shape);

    CHECK(ell.type() == "LcEllipsoid");
    CHECK(ell.has_mask());
    CHECK(ell.ndim() == 2);

    auto mask = ell.get_mask();
    // Center should be included.
    auto bb = ell.bounding_box();
    std::int64_t cx = 10 - bb.start()[0];
    std::int64_t cy = 10 - bb.start()[1];
    CHECK(mask.at(IPosition{cx, cy}) == true);
}

static void test_lcellipsoid_roundtrip() {
    IPosition lat_shape{20, 20};
    LcEllipsoid ell({5.0, 5.0}, {2.0, 3.0}, lat_shape);
    auto rec = ell.to_record();
    auto restored = LcEllipsoid::from_record(rec, lat_shape);
    CHECK(restored->center()[0] == 5.0);
    CHECK(restored->semi_axes()[1] == 3.0);
}

// ── LcPolygon tests ──────────────────────────────────────────────────

static void test_lcpolygon_basic() {
    IPosition lat_shape{20, 20};
    // Triangle: (5,5), (15,5), (10,15)
    LcPolygon poly({5.0, 15.0, 10.0}, {5.0, 5.0, 15.0}, lat_shape);

    CHECK(poly.type() == "LcPolygon");
    CHECK(poly.has_mask());
    CHECK(poly.ndim() == 2);

    auto mask = poly.get_mask();
    // Center (10,8) should be inside the triangle.
    auto bb = poly.bounding_box();
    // Check that mask has some true and some false values.
    bool has_true = false;
    bool has_false = false;
    for (std::size_t i = 0; i < mask.nelements(); ++i) {
        if (mask.flat()[i]) has_true = true;
        else has_false = true;
    }
    CHECK(has_true);
    CHECK(has_false);
}

// ── LcMask tests ─────────────────────────────────────────────────────

static void test_lcmask_basic() {
    IPosition lat_shape{4, 4};
    LatticeArray<bool> mask(lat_shape, true);
    mask.make_unique();
    mask.put(IPosition{1, 1}, false);

    LcMask lm(std::move(mask), lat_shape);
    CHECK(lm.type() == "LcMask");
    CHECK(lm.has_mask());
    auto m = lm.get_mask();
    CHECK(m.at(IPosition{0, 0}) == true);
    CHECK(m.at(IPosition{1, 1}) == false);
}

// ── Compound region tests ────────────────────────────────────────────

static void test_lcunion() {
    IPosition lat_shape{10, 10};
    std::vector<std::unique_ptr<LcRegion>> regions;
    regions.push_back(
        std::make_unique<LcBox>(IPosition{0, 0}, IPosition{2, 2}, lat_shape));
    regions.push_back(
        std::make_unique<LcBox>(IPosition{5, 5}, IPosition{7, 7}, lat_shape));

    LcUnion u(std::move(regions), lat_shape);
    CHECK(u.type() == "LcUnion");
    CHECK(u.has_mask());
    CHECK(u.n_regions() == 2);

    auto mask = u.get_mask();
    // (0,0) is in first box → true.
    auto bb = u.bounding_box();
    IPosition p00{0 - bb.start()[0], 0 - bb.start()[1]};
    CHECK(mask.at(p00) == true);
    // (3,3) is in neither box → false.
    IPosition p33{3 - bb.start()[0], 3 - bb.start()[1]};
    CHECK(mask.at(p33) == false);
}

static void test_lcintersection() {
    IPosition lat_shape{10, 10};
    std::vector<std::unique_ptr<LcRegion>> regions;
    regions.push_back(
        std::make_unique<LcBox>(IPosition{0, 0}, IPosition{5, 5}, lat_shape));
    regions.push_back(
        std::make_unique<LcBox>(IPosition{3, 3}, IPosition{9, 9}, lat_shape));

    LcIntersection inter(std::move(regions), lat_shape);
    CHECK(inter.type() == "LcIntersection");

    auto mask = inter.get_mask();
    auto bb = inter.bounding_box();
    // bb should be (3,3)-(5,5), a 3x3 region.
    CHECK(bb.start()[0] == 3);
    CHECK(bb.start()[1] == 3);
    CHECK(bb.length()[0] == 3);
    CHECK(bb.length()[1] == 3);
    // All pixels in this bb are in both boxes.
    CHECK(mask.at(IPosition{0, 0}) == true);
}

static void test_lcdifference() {
    IPosition lat_shape{10, 10};
    auto box1 = std::make_unique<LcBox>(
        IPosition{0, 0}, IPosition{5, 5}, lat_shape);
    auto box2 = std::make_unique<LcBox>(
        IPosition{3, 3}, IPosition{5, 5}, lat_shape);

    LcDifference diff(std::move(box1), std::move(box2), lat_shape);
    CHECK(diff.type() == "LcDifference");

    auto mask = diff.get_mask();
    auto bb = diff.bounding_box();
    // (0,0) relative to bb → in box1 but not box2 → true.
    CHECK(mask.at(IPosition{0, 0}) == true);
    // (3,3) is in both → false.
    CHECK(mask.at(IPosition{3, 3}) == false);
}

static void test_lccomplement() {
    IPosition lat_shape{6, 6};
    auto box = std::make_unique<LcBox>(
        IPosition{1, 1}, IPosition{3, 3}, lat_shape);

    LcComplement comp(std::move(box), lat_shape);
    CHECK(comp.type() == "LcComplement");

    auto mask = comp.get_mask();
    // (0,0) not in box → true (complement).
    CHECK(mask.at(IPosition{0, 0}) == true);
    // (2,2) in box → false.
    CHECK(mask.at(IPosition{2, 2}) == false);
}

// ── LatticeRegion tests ──────────────────────────────────────────────

static void test_lattice_region_basic() {
    IPosition lat_shape{10, 10};
    auto box = std::make_unique<LcBox>(
        IPosition{1, 2}, IPosition{4, 5}, lat_shape);

    LatticeRegion lr(std::move(box));
    CHECK(lr.has_region());
    CHECK(lr.region().type() == "LcBox");
    CHECK(!lr.has_slicer());
}

static void test_lattice_region_roundtrip() {
    IPosition lat_shape{10, 10};
    auto box = std::make_unique<LcBox>(
        IPosition{1, 2}, IPosition{4, 5}, lat_shape);

    LatticeRegion lr(std::move(box));
    auto rec = lr.to_record();
    auto restored = LatticeRegion::from_record(rec, lat_shape);
    CHECK(restored.has_region());
    CHECK(restored.region().type() == "LcBox");
}

// ── LcRegion::from_record dispatch ───────────────────────────────────

static void test_region_from_record_dispatch() {
    IPosition lat_shape{10, 10};
    LcBox box(IPosition{0, 0}, IPosition{3, 3}, lat_shape);
    auto rec = box.to_record();
    auto restored = LcRegion::from_record(rec, lat_shape);
    CHECK(restored->type() == "LcBox");
}

static void test_region_from_record_unknown_type() {
    Record rec;
    rec.set("type", RecordValue(std::string("LcUnknown")));
    bool threw = false;
    try {
        auto r = LcRegion::from_record(rec, IPosition{10, 10});
        (void)r;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    // LcBox
    test_lcbox_basic();
    test_lcbox_from_slicer();
    test_lcbox_mask();
    test_lcbox_roundtrip();
    test_lcbox_clone();

    // LcPixelSet
    test_lcpixelset_basic();

    // LcEllipsoid
    test_lcellipsoid_basic();
    test_lcellipsoid_roundtrip();

    // LcPolygon
    test_lcpolygon_basic();

    // LcMask
    test_lcmask_basic();

    // Compounds
    test_lcunion();
    test_lcintersection();
    test_lcdifference();
    test_lccomplement();

    // LatticeRegion
    test_lattice_region_basic();
    test_lattice_region_roundtrip();

    // Dispatch
    test_region_from_record_dispatch();
    test_region_from_record_unknown_type();

    std::cout << "lattice_region_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
