// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/image_region.hpp"
#include "casacore_mini/direction_coordinate.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
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

// ── Helper: build a simple 2D coordinate system ─────────────────────

static CoordinateSystem make_2d_cs() {
    CoordinateSystem cs;
    // ref at pixel (50,50), inc = -1e-3 / +1e-3 radians per pixel.
    auto dc = std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {}},
        0.0, 0.0, -1e-3, 1e-3,
        std::vector<double>{1, 0, 0, 1}, 50.0, 50.0);
    cs.add_coordinate(std::move(dc));
    return cs;
}

// ── WcBox tests ─────────────────────────────────────────────────────

static void test_wcbox_basic() {
    WcBox box({-10.0, -10.0}, {10.0, 10.0},
              {"Right Ascension", "Declination"},
              {"rad", "rad"});
    CHECK(box.type() == "WcBox");
    CHECK(box.ndim() == 2);
    CHECK(box.blc()[0] == -10.0);
    CHECK(box.trc()[1] == 10.0);
}

static void test_wcbox_clone() {
    WcBox box({-5.0, -5.0}, {5.0, 5.0},
              {"Right Ascension", "Declination"},
              {"rad", "rad"});
    auto cloned = box.clone();
    CHECK(cloned->type() == "WcBox");
    CHECK(cloned->ndim() == 2);
}

static void test_wcbox_roundtrip() {
    WcBox box({-3.0, -2.0}, {3.0, 2.0},
              {"Right Ascension", "Declination"},
              {"rad", "rad"});
    auto rec = box.to_record();
    auto restored = WcBox::from_record(rec);
    CHECK(restored->blc()[0] == -3.0);
    CHECK(restored->trc()[1] == 2.0);
}

static void test_wcbox_to_lc() {
    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};

    // CS: ref_pix=50, inc=-1e-3/+1e-3 rad/pixel.
    // Box from -0.01 to 0.01 rad → should span ~20 pixels centered at 50.
    WcBox box({-0.01, -0.01}, {0.01, 0.01},
              {"Right Ascension", "Declination"},
              {"rad", "rad"});

    auto lc = box.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcBox");
    auto& bb = lc->bounding_box();
    CHECK(bb.length()[0] > 0);
    CHECK(bb.length()[1] > 0);
}

// ── WcEllipsoid tests ───────────────────────────────────────────────

static void test_wcellipsoid_basic() {
    WcEllipsoid ell({0.0, 0.0}, {5.0, 3.0},
                    {"Right Ascension", "Declination"},
                    {"rad", "rad"});
    CHECK(ell.type() == "WcEllipsoid");
    CHECK(ell.ndim() == 2);
    CHECK(ell.center()[0] == 0.0);
    CHECK(ell.radii()[1] == 3.0);
}

static void test_wcellipsoid_roundtrip() {
    WcEllipsoid ell({1.0, 2.0}, {3.0, 4.0},
                    {"Right Ascension", "Declination"},
                    {"rad", "rad"});
    auto rec = ell.to_record();
    auto restored = WcEllipsoid::from_record(rec);
    CHECK(restored->center()[0] == 1.0);
    CHECK(restored->radii()[1] == 4.0);
}

static void test_wcellipsoid_to_lc() {
    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};

    WcEllipsoid ell({0.0, 0.0}, {0.005, 0.003},
                    {"Right Ascension", "Declination"},
                    {"rad", "rad"});

    auto lc = ell.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcEllipsoid");
    CHECK(lc->has_mask());
}

// ── WcPolygon tests ─────────────────────────────────────────────────

static void test_wcpolygon_basic() {
    WcPolygon poly({-5.0, 5.0, 0.0}, {-5.0, -5.0, 5.0},
                   "Right Ascension", "Declination",
                   "rad", "rad");
    CHECK(poly.type() == "WcPolygon");
    CHECK(poly.ndim() == 2);
    CHECK(poly.x().size() == 3);
}

static void test_wcpolygon_roundtrip() {
    WcPolygon poly({-3.0, 3.0, 0.0}, {-3.0, -3.0, 3.0},
                   "Right Ascension", "Declination",
                   "rad", "rad");
    auto rec = poly.to_record();
    auto restored = WcPolygon::from_record(rec);
    CHECK(restored->x().size() == 3);
    CHECK(restored->y()[2] == 3.0);
}

static void test_wcpolygon_to_lc() {
    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};

    WcPolygon poly({-0.005, 0.005, 0.0}, {-0.005, -0.005, 0.005},
                   "Right Ascension", "Declination",
                   "rad", "rad");
    auto lc = poly.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcPolygon");
    CHECK(lc->has_mask());
}

// ── Compound WC region tests ────────────────────────────────────────

static void test_wc_union() {
    std::vector<std::unique_ptr<WcRegion>> regions;
    regions.push_back(std::make_unique<WcBox>(
        std::vector<double>{-0.02, -0.02}, std::vector<double>{-0.01, -0.01},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"}));
    regions.push_back(std::make_unique<WcBox>(
        std::vector<double>{0.01, 0.01}, std::vector<double>{0.02, 0.02},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"}));

    WcUnion u(std::move(regions));
    CHECK(u.type() == "WcUnion");
    CHECK(u.n_regions() == 2);
    CHECK(u.ndim() == 2);

    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};
    auto lc = u.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcUnion");
}

static void test_wc_intersection() {
    std::vector<std::unique_ptr<WcRegion>> regions;
    regions.push_back(std::make_unique<WcBox>(
        std::vector<double>{-0.02, -0.02}, std::vector<double>{0.01, 0.01},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"}));
    regions.push_back(std::make_unique<WcBox>(
        std::vector<double>{-0.01, -0.01}, std::vector<double>{0.02, 0.02},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"}));

    WcIntersection inter(std::move(regions));
    CHECK(inter.type() == "WcIntersection");
    CHECK(inter.n_regions() == 2);

    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};
    auto lc = inter.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcIntersection");
}

static void test_wc_difference() {
    auto box1 = std::make_unique<WcBox>(
        std::vector<double>{-0.02, -0.02}, std::vector<double>{0.02, 0.02},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});
    auto box2 = std::make_unique<WcBox>(
        std::vector<double>{-0.01, -0.01}, std::vector<double>{0.01, 0.01},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});

    WcDifference diff(std::move(box1), std::move(box2));
    CHECK(diff.type() == "WcDifference");

    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};
    auto lc = diff.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcDifference");
}

static void test_wc_complement() {
    auto box = std::make_unique<WcBox>(
        std::vector<double>{-0.01, -0.01}, std::vector<double>{0.01, 0.01},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});

    WcComplement comp(std::move(box));
    CHECK(comp.type() == "WcComplement");

    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};
    auto lc = comp.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcComplement");
}

// ── ImageRegion tests ───────────────────────────────────────────────

static void test_image_region_lc() {
    IPosition lat_shape{10, 10};
    auto box = std::make_unique<LcBox>(IPosition{1, 1}, IPosition{5, 5},
                                       lat_shape);
    ImageRegion ir(std::move(box));
    CHECK(ir.kind() == RegionKind::lc_region);
    CHECK(ir.has_lc_region());
    CHECK(!ir.has_wc_region());
    CHECK(ir.as_lc_region().type() == "LcBox");
}

static void test_image_region_wc() {
    auto wcbox = std::make_unique<WcBox>(
        std::vector<double>{-5.0, -5.0}, std::vector<double>{5.0, 5.0},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});
    ImageRegion ir(std::move(wcbox));
    CHECK(ir.kind() == RegionKind::wc_region);
    CHECK(ir.has_wc_region());
    CHECK(ir.as_wc_region().type() == "WcBox");
}

static void test_image_region_slicer() {
    ImageRegion ir(Slicer(IPosition{2, 3}, IPosition{5, 6}));
    CHECK(ir.kind() == RegionKind::slicer);
    CHECK(ir.has_slicer());
    CHECK(ir.as_slicer().start()[0] == 2);
}

static void test_image_region_lc_roundtrip() {
    IPosition lat_shape{10, 10};
    auto box = std::make_unique<LcBox>(IPosition{1, 2}, IPosition{4, 6},
                                       lat_shape);
    ImageRegion ir(std::move(box));
    auto rec = ir.to_record();
    auto restored = ImageRegion::from_record(rec, lat_shape);
    CHECK(restored.kind() == RegionKind::lc_region);
    CHECK(restored.has_lc_region());
    CHECK(restored.as_lc_region().type() == "LcBox");
}

static void test_image_region_wc_roundtrip() {
    auto wcbox = std::make_unique<WcBox>(
        std::vector<double>{-3.0, -2.0}, std::vector<double>{3.0, 2.0},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});
    ImageRegion ir(std::move(wcbox));
    auto rec = ir.to_record();
    IPosition lat_shape{100, 100};
    auto restored = ImageRegion::from_record(rec, lat_shape);
    CHECK(restored.kind() == RegionKind::wc_region);
    CHECK(restored.has_wc_region());
    CHECK(restored.as_wc_region().type() == "WcBox");
}

static void test_image_region_slicer_roundtrip() {
    ImageRegion ir(Slicer(IPosition{2, 3}, IPosition{5, 6}));
    auto rec = ir.to_record();
    IPosition lat_shape{20, 20};
    auto restored = ImageRegion::from_record(rec, lat_shape);
    CHECK(restored.kind() == RegionKind::slicer);
    CHECK(restored.as_slicer().start()[0] == 2);
    CHECK(restored.as_slicer().length()[1] == 6);
}

static void test_image_region_to_lc() {
    auto cs = make_2d_cs();
    IPosition lat_shape{100, 100};

    // WC region converted to LC.
    auto wcbox = std::make_unique<WcBox>(
        std::vector<double>{-0.01, -0.01}, std::vector<double>{0.01, 0.01},
        std::vector<std::string>{"Right Ascension", "Declination"},
        std::vector<std::string>{"rad", "rad"});
    ImageRegion ir(std::move(wcbox));
    auto lc = ir.to_lc_region(cs, lat_shape);
    CHECK(lc != nullptr);
    CHECK(lc->type() == "LcBox");

    // Slicer converted to LcBox.
    ImageRegion ir2(Slicer(IPosition{10, 10}, IPosition{20, 20}));
    auto lc2 = ir2.to_lc_region(cs, lat_shape);
    CHECK(lc2 != nullptr);
    CHECK(lc2->type() == "LcBox");
}

// ── RegionHandler tests ─────────────────────────────────────────────

static void test_region_handler_basic() {
    RegionHandler rh;
    CHECK(rh.n_regions() == 0);

    IPosition lat_shape{10, 10};
    rh.define_region("box1",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{0, 0}, IPosition{3, 3}, lat_shape)));
    CHECK(rh.n_regions() == 1);
    CHECK(rh.has_region("box1"));
    CHECK(!rh.has_region("box2"));
    CHECK(rh.get_region("box1").as_lc_region().type() == "LcBox");
}

static void test_region_handler_remove_rename() {
    RegionHandler rh;
    IPosition lat_shape{10, 10};
    rh.define_region("r1",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{0, 0}, IPosition{3, 3}, lat_shape)));
    rh.define_region("r2",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{1, 1}, IPosition{4, 4}, lat_shape)));
    CHECK(rh.n_regions() == 2);

    rh.rename_region("r1", "renamed");
    CHECK(!rh.has_region("r1"));
    CHECK(rh.has_region("renamed"));
    CHECK(rh.n_regions() == 2);

    rh.remove_region("renamed");
    CHECK(rh.n_regions() == 1);
    CHECK(!rh.has_region("renamed"));
}

static void test_region_handler_default_mask() {
    RegionHandler rh;
    CHECK(rh.default_mask().empty());
    rh.set_default_mask("mask0");
    CHECK(rh.default_mask() == "mask0");

    // Renaming the default mask updates it.
    IPosition lat_shape{10, 10};
    rh.define_region("mask0",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{0, 0}, IPosition{3, 3}, lat_shape)));
    rh.rename_region("mask0", "mask1");
    CHECK(rh.default_mask() == "mask1");
}

static void test_region_handler_roundtrip() {
    RegionHandler rh;
    IPosition lat_shape{10, 10};
    rh.define_region("box1",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{1, 2}, IPosition{4, 5}, lat_shape)));
    rh.set_default_mask("box1");

    auto rec = rh.to_record();
    RegionHandler rh2;
    rh2.from_record(rec, lat_shape);
    CHECK(rh2.n_regions() == 1);
    CHECK(rh2.has_region("box1"));
    CHECK(rh2.default_mask() == "box1");
    CHECK(rh2.get_region("box1").as_lc_region().type() == "LcBox");
}

static void test_region_handler_names() {
    RegionHandler rh;
    IPosition lat_shape{10, 10};
    rh.define_region("c",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{0, 0}, IPosition{3, 3}, lat_shape)));
    rh.define_region("a",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{1, 1}, IPosition{4, 4}, lat_shape)));
    rh.define_region("b",
                     ImageRegion(std::make_unique<LcBox>(
                         IPosition{2, 2}, IPosition{5, 5}, lat_shape)));
    auto names = rh.region_names();
    CHECK(names.size() == 3);
    // Should be sorted.
    CHECK(names[0] == "a");
    CHECK(names[1] == "b");
    CHECK(names[2] == "c");
}

// ── RegionManager tests ─────────────────────────────────────────────

static void test_region_manager_factories() {
    auto box = RegionManager::wc_box(
        {-5.0, -5.0}, {5.0, 5.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"});
    CHECK(box->type() == "WcBox");

    auto ell = RegionManager::wc_ellipsoid(
        {0.0, 0.0}, {3.0, 2.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"});
    CHECK(ell->type() == "WcEllipsoid");

    auto poly = RegionManager::wc_polygon(
        {-3.0, 3.0, 0.0}, {-3.0, -3.0, 3.0},
        "Right Ascension", "Declination", "rad", "rad");
    CHECK(poly->type() == "WcPolygon");
}

static void test_region_manager_compound() {
    std::vector<std::unique_ptr<WcRegion>> regions;
    regions.push_back(RegionManager::wc_box(
        {-5.0, -5.0}, {5.0, 5.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"}));
    regions.push_back(RegionManager::wc_box(
        {0.0, 0.0}, {10.0, 10.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"}));

    auto u = RegionManager::make_union(std::move(regions));
    CHECK(u->type() == "WcUnion");

    std::vector<std::unique_ptr<WcRegion>> regions2;
    regions2.push_back(RegionManager::wc_box(
        {-5.0, -5.0}, {5.0, 5.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"}));
    regions2.push_back(RegionManager::wc_box(
        {0.0, 0.0}, {10.0, 10.0},
        {"Right Ascension", "Declination"}, {"rad", "rad"}));
    auto inter = RegionManager::make_intersection(std::move(regions2));
    CHECK(inter->type() == "WcIntersection");

    auto diff = RegionManager::make_difference(
        RegionManager::wc_box({-5.0, -5.0}, {5.0, 5.0},
                              {"Right Ascension", "Declination"}, {"rad", "rad"}),
        RegionManager::wc_box({0.0, 0.0}, {3.0, 3.0},
                              {"Right Ascension", "Declination"}, {"rad", "rad"}));
    CHECK(diff->type() == "WcDifference");

    auto comp = RegionManager::make_complement(
        RegionManager::wc_box({-5.0, -5.0}, {5.0, 5.0},
                              {"Right Ascension", "Declination"}, {"rad", "rad"}));
    CHECK(comp->type() == "WcComplement");
}

// ── WcRegion::from_record dispatch ──────────────────────────────────

static void test_wc_from_record_dispatch() {
    WcBox box({-1.0, -1.0}, {1.0, 1.0},
              {"Right Ascension", "Declination"},
              {"rad", "rad"});
    auto rec = box.to_record();
    auto restored = WcRegion::from_record(rec);
    CHECK(restored->type() == "WcBox");
}

static void test_wc_from_record_unknown_type() {
    Record rec;
    rec.set("type", RecordValue(std::string("WcUnknown")));
    bool threw = false;
    try {
        auto r = WcRegion::from_record(rec);
        (void)r;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    // WcBox
    test_wcbox_basic();
    test_wcbox_clone();
    test_wcbox_roundtrip();
    test_wcbox_to_lc();

    // WcEllipsoid
    test_wcellipsoid_basic();
    test_wcellipsoid_roundtrip();
    test_wcellipsoid_to_lc();

    // WcPolygon
    test_wcpolygon_basic();
    test_wcpolygon_roundtrip();
    test_wcpolygon_to_lc();

    // Compound WC regions
    test_wc_union();
    test_wc_intersection();
    test_wc_difference();
    test_wc_complement();

    // ImageRegion
    test_image_region_lc();
    test_image_region_wc();
    test_image_region_slicer();
    test_image_region_lc_roundtrip();
    test_image_region_wc_roundtrip();
    test_image_region_slicer_roundtrip();
    test_image_region_to_lc();

    // RegionHandler
    test_region_handler_basic();
    test_region_handler_remove_rename();
    test_region_handler_default_mask();
    test_region_handler_roundtrip();
    test_region_handler_names();

    // RegionManager
    test_region_manager_factories();
    test_region_manager_compound();

    // Dispatch
    test_wc_from_record_dispatch();
    test_wc_from_record_unknown_type();

    std::cout << "image_region_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
