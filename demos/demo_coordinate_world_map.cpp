// demo_coordinate_world_map.cpp -- Phase 8: world/pixel map transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/coordinates/Coordinates/test/dWorldMap.cc
//     CoordinateSystem cSys1 = CoordinateUtil::defaultCoords2D();
//     CoordinateSystem cSys2 = CoordinateUtil::defaultCoords3D();
//     Bool ok = cSys1.worldMap(wmap, wtranspose, refChange, cSys2);
//     Bool ok2 = cSys1.pixelMap(pmap, ptranspose, cSys2);
//
//     cSys2.transpose(worldOrder, pixelOrder);
//     ok = cSys1.worldMap(wmap, wtranspose, refChange, cSys2);
//
// This casacore-mini demo transliterates those mapping checks by computing
// world/pixel axis correspondences between CoordinateSystem instances.

#include <casacore_mini/coordinate_system.hpp>
#include <casacore_mini/direction_coordinate.hpp>
#include <casacore_mini/projection.hpp>
#include <casacore_mini/quantity.hpp>
#include <casacore_mini/spectral_coordinate.hpp>

#include "demo_check.hpp"
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace casacore_mini;

namespace {

CoordinateSystem make_default_coords2d() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
        Quantity(135.0, "deg").get_value("rad"), Quantity(60.0, "deg").get_value("rad"),
        Quantity(-1.0, "deg").get_value("rad"), Quantity(1.0, "deg").get_value("rad"),
        std::vector<double>{}, 128.0, 128.0));
    return cs;
}

CoordinateSystem make_default_coords3d() {
    CoordinateSystem cs = make_default_coords2d();
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::topo, 1400.0e6, 20.0e3, 0.0));
    return cs;
}

CoordinateSystem make_reordered_coords3d() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::topo, 1400.0e6, 20.0e3, 0.0));
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
        Quantity(135.0, "deg").get_value("rad"), Quantity(60.0, "deg").get_value("rad"),
        Quantity(-1.0, "deg").get_value("rad"), Quantity(1.0, "deg").get_value("rad"),
        std::vector<double>{}, 128.0, 128.0));
    return cs;
}

std::vector<int> build_transpose(const std::vector<int>& map, std::size_t target_size) {
    std::vector<int> transpose(target_size, -1);
    for (std::size_t i = 0; i < map.size(); ++i) {
        const auto m = map[i];
        if (m >= 0 && static_cast<std::size_t>(m) < target_size) {
            transpose[static_cast<std::size_t>(m)] = static_cast<int>(i);
        }
    }
    return transpose;
}

std::vector<int> build_world_map(const CoordinateSystem& from, const CoordinateSystem& to) {
    std::vector<int> map(from.n_world_axes(), -1);
    std::vector<bool> used(to.n_world_axes(), false);

    for (std::size_t wf = 0; wf < from.n_world_axes(); ++wf) {
        const auto floc = from.find_world_axis(wf);
        DEMO_CHECK(floc.has_value());
        const auto& fcoord = from.coordinate(floc->first);
        const auto fnames = fcoord.world_axis_names();
        const auto fname = fnames[floc->second];

        for (std::size_t wt = 0; wt < to.n_world_axes(); ++wt) {
            if (used[wt]) {
                continue;
            }
            const auto tloc = to.find_world_axis(wt);
            DEMO_CHECK(tloc.has_value());
            const auto& tcoord = to.coordinate(tloc->first);
            const auto tnames = tcoord.world_axis_names();
            if (fcoord.type() == tcoord.type() && fname == tnames[tloc->second]) {
                map[wf] = static_cast<int>(wt);
                used[wt] = true;
                break;
            }
        }
    }
    return map;
}

std::vector<int> build_pixel_map(const CoordinateSystem& from, const CoordinateSystem& to) {
    std::vector<int> map(from.n_pixel_axes(), -1);
    std::vector<bool> used(to.n_pixel_axes(), false);

    for (std::size_t pf = 0; pf < from.n_pixel_axes(); ++pf) {
        const auto floc = from.find_pixel_axis(pf);
        DEMO_CHECK(floc.has_value());
        const auto& fcoord = from.coordinate(floc->first);

        for (std::size_t pt = 0; pt < to.n_pixel_axes(); ++pt) {
            if (used[pt]) {
                continue;
            }
            const auto tloc = to.find_pixel_axis(pt);
            DEMO_CHECK(tloc.has_value());
            const auto& tcoord = to.coordinate(tloc->first);
            if (fcoord.type() == tcoord.type() && floc->second == tloc->second) {
                map[pf] = static_cast<int>(pt);
                used[pt] = true;
                break;
            }
        }
    }
    return map;
}

void print_vec(const char* name, const std::vector<int>& vals) {
    std::cout << "  " << name << " = [";
    for (std::size_t i = 0; i < vals.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << vals[i];
    }
    std::cout << "]\n";
}

void run_case(const std::string& label, const CoordinateSystem& lhs, const CoordinateSystem& rhs,
              const std::vector<int>& expected_world, const std::vector<int>& expected_pixel) {
    std::cout << "\n--- " << label << " ---\n";
    auto wmap = build_world_map(lhs, rhs);
    auto pmap = build_pixel_map(lhs, rhs);
    auto wtranspose = build_transpose(wmap, rhs.n_world_axes());
    auto ptranspose = build_transpose(pmap, rhs.n_pixel_axes());

    print_vec("world_map", wmap);
    print_vec("pixel_map", pmap);
    print_vec("world_transpose", wtranspose);
    print_vec("pixel_transpose", ptranspose);

    DEMO_CHECK(wmap == expected_world);
    DEMO_CHECK(pmap == expected_pixel);
}

} // namespace

int main() {
    try {
        std::cout << "=== casacore-mini Demo: Coordinate World/Pixel Maps (Phase 8) ===\n";

        const auto c2d = make_default_coords2d();
        const auto c3d = make_default_coords3d();
        const auto c3d_reordered = make_reordered_coords3d();
        const CoordinateSystem c0d;

        run_case("2D [ra,dec] vs 0D", c2d, c0d, {-1, -1}, {-1, -1});
        run_case("0D vs 2D [ra,dec]", c0d, c2d, {}, {});
        run_case("3D [ra,dec,spec] vs 3D [ra,dec,spec]", c3d, c3d, {0, 1, 2}, {0, 1, 2});
        run_case("2D [ra,dec] vs 3D [ra,dec,spec]", c2d, c3d, {0, 1}, {0, 1});
        run_case("3D [ra,dec,spec] vs 2D [ra,dec]", c3d, c2d, {0, 1, -1}, {0, 1, -1});
        run_case("3D [ra,dec,spec] vs 3D [spec,ra,dec]", c3d, c3d_reordered, {1, 2, 0},
                 {1, 2, 0});

        std::cout << "\n=== Coordinate World/Pixel Map demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
