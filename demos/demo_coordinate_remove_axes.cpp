// demo_coordinate_remove_axes.cpp -- Phase 8: remove-axes transliteration demo
//
// casacore-original reference excerpts:
//   Source: casacore-original/coordinates/Coordinates/test/dRemoveAxes.cc
//     CoordinateSystem cSys = CoordinateUtil::defaultCoords3D();
//     Vector<Int> list(1); list(0) = 2;
//     Bool remove = True;
//     CoordinateUtil::removeAxes(cSys, worldReplacement, list, remove);
//
//     Vector<Int> pixList(2); pixList(0) = 0; pixList(1) = 2;
//     CoordinateUtil::removePixelAxes(cSys, pixelReplacement, pixList, remove);
//
// This casacore-mini demo transliterates those workflows using currently
// available public APIs by removing the owning coordinate for selected axes.

#include <casacore_mini/coordinate_system.hpp>
#include <casacore_mini/direction_coordinate.hpp>
#include <casacore_mini/projection.hpp>
#include <casacore_mini/quantity.hpp>
#include <casacore_mini/spectral_coordinate.hpp>

#include "demo_check.hpp"
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace casacore_mini;

namespace {

CoordinateSystem make_default_coords3d() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
        Quantity(135.0, "deg").get_value("rad"), Quantity(60.0, "deg").get_value("rad"),
        Quantity(-1.0, "deg").get_value("rad"), Quantity(1.0, "deg").get_value("rad"),
        std::vector<double>{}, 128.0, 128.0));
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::topo, 1400.0e6, 20.0e3, 0.0));
    return cs;
}

CoordinateSystem remove_coords(const CoordinateSystem& src, const std::set<std::size_t>& drop_coords) {
    CoordinateSystem out;
    for (std::size_t ci = 0; ci < src.n_coordinates(); ++ci) {
        if (drop_coords.count(ci) == 0) {
            out.add_coordinate(src.coordinate(ci).clone());
        }
    }
    return out;
}

CoordinateSystem remove_world_axes(const CoordinateSystem& src, const std::vector<std::size_t>& axes) {
    std::set<std::size_t> drop_coords;
    for (const auto axis : axes) {
        const auto loc = src.find_world_axis(axis);
        if (loc.has_value()) {
            drop_coords.insert(loc->first);
        }
    }
    return remove_coords(src, drop_coords);
}

CoordinateSystem remove_pixel_axes(const CoordinateSystem& src, const std::vector<std::size_t>& axes) {
    std::set<std::size_t> drop_coords;
    for (const auto axis : axes) {
        const auto loc = src.find_pixel_axis(axis);
        if (loc.has_value()) {
            drop_coords.insert(loc->first);
        }
    }
    return remove_coords(src, drop_coords);
}

std::vector<std::string> world_axis_names(const CoordinateSystem& cs) {
    std::vector<std::string> names;
    names.reserve(cs.n_world_axes());
    for (std::size_t w = 0; w < cs.n_world_axes(); ++w) {
        const auto loc = cs.find_world_axis(w);
        DEMO_CHECK(loc.has_value());
        const auto coord_names = cs.coordinate(loc->first).world_axis_names();
        names.push_back(coord_names[loc->second]);
    }
    return names;
}

void print_summary(const std::string& label, const CoordinateSystem& cs) {
    std::cout << "\n--- " << label << " ---\n";
    std::cout << "  coordinates: " << cs.n_coordinates() << "\n";
    std::cout << "  pixel axes:  " << cs.n_pixel_axes() << "\n";
    std::cout << "  world axes:  " << cs.n_world_axes() << "\n";
    const auto names = world_axis_names(cs);
    std::cout << "  world axis names: [";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << names[i];
    }
    std::cout << "]\n";
}

} // namespace

int main() {
    try {
        std::cout << "=== casacore-mini Demo: Coordinate Remove Axes (Phase 8) ===\n";

        const auto base = make_default_coords3d();
        print_summary("base 3D [ra, dec, frequency]", base);
        DEMO_CHECK(base.n_coordinates() == 2);
        DEMO_CHECK(base.n_world_axes() == 3);
        DEMO_CHECK(base.n_pixel_axes() == 3);

        const auto no_spec = remove_world_axes(base, {2});
        print_summary("remove world axis [2]", no_spec);
        DEMO_CHECK(no_spec.n_coordinates() == 1);
        DEMO_CHECK(no_spec.n_world_axes() == 2);
        DEMO_CHECK(no_spec.n_pixel_axes() == 2);

        const auto no_dir_no_spec = remove_world_axes(base, {0, 2});
        print_summary("remove world axes [0, 2]", no_dir_no_spec);
        DEMO_CHECK(no_dir_no_spec.n_coordinates() == 0);
        DEMO_CHECK(no_dir_no_spec.n_world_axes() == 0);
        DEMO_CHECK(no_dir_no_spec.n_pixel_axes() == 0);

        const auto remove_pix0 = remove_pixel_axes(base, {0});
        print_summary("remove pixel axis [0]", remove_pix0);
        DEMO_CHECK(remove_pix0.n_coordinates() == 1);
        DEMO_CHECK(remove_pix0.n_world_axes() == 1);
        DEMO_CHECK(remove_pix0.n_pixel_axes() == 1);

        const auto remove_pix01 = remove_pixel_axes(base, {0, 1});
        print_summary("remove pixel axes [0, 1]", remove_pix01);
        DEMO_CHECK(remove_pix01.n_coordinates() == 1);
        DEMO_CHECK(remove_pix01.n_world_axes() == 1);
        DEMO_CHECK(remove_pix01.n_pixel_axes() == 1);

        const auto remove_pix12 = remove_pixel_axes(base, {1, 2});
        print_summary("remove pixel axes [1, 2]", remove_pix12);
        DEMO_CHECK(remove_pix12.n_coordinates() == 0);
        DEMO_CHECK(remove_pix12.n_world_axes() == 0);
        DEMO_CHECK(remove_pix12.n_pixel_axes() == 0);

        std::cout << "\nNOTE: casacore-original removeAxes/removePixelAxes operate at axis granularity;\n"
                  << "      current casacore-mini public APIs in this demo remove the owning\n"
                  << "      coordinate for each selected axis.\n";

        std::cout << "\n=== Coordinate Remove Axes demo passed. ===\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
