#include "casacore_mini/coordinate_util.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/fits_coordinate_util.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-6;
constexpr double kDeg2Rad = M_PI / 180.0;

[[maybe_unused]] bool near(double a, double b, double tol = kTol) {
    return std::abs(a - b) < tol * std::max(1.0, std::abs(a));
}

bool test_ra_dec_freq_stokes() {
    Record fits;
    fits.set("NAXIS", RecordValue(std::int32_t(4)));
    fits.set("CTYPE1", RecordValue(std::string("RA---SIN")));
    fits.set("CTYPE2", RecordValue(std::string("DEC--SIN")));
    fits.set("CTYPE3", RecordValue(std::string("FREQ")));
    fits.set("CTYPE4", RecordValue(std::string("STOKES")));

    fits.set("CRVAL1", RecordValue(180.0));
    fits.set("CRVAL2", RecordValue(45.0));
    fits.set("CRVAL3", RecordValue(1.42e9));
    fits.set("CRVAL4", RecordValue(1.0));

    fits.set("CRPIX1", RecordValue(129.0)); // FITS 1-based
    fits.set("CRPIX2", RecordValue(129.0));
    fits.set("CRPIX3", RecordValue(1.0));
    fits.set("CRPIX4", RecordValue(1.0));

    fits.set("CDELT1", RecordValue(-1.0 / 3600.0));
    fits.set("CDELT2", RecordValue(1.0 / 3600.0));
    fits.set("CDELT3", RecordValue(1e6));
    fits.set("CDELT4", RecordValue(1.0));

    fits.set("NAXIS4", RecordValue(std::int32_t(4)));

    CoordinateSystem cs = coordinate_system_from_fits_header(fits);

    auto dir_idx = find_direction_coordinate(cs);
    assert(dir_idx.has_value());
    [[maybe_unused]] const auto& dc =
        static_cast<const DirectionCoordinate&>(cs.coordinate(*dir_idx));
    assert(dc.ref_frame() == DirectionRef::j2000);
    assert(dc.projection().type == ProjectionType::sin);

    [[maybe_unused]] auto spec_idx = find_spectral_coordinate(cs);
    assert(spec_idx.has_value());

    [[maybe_unused]] auto stokes_idx = find_stokes_coordinate(cs);
    assert(stokes_idx.has_value());

    // Verify direction reference pixel maps to reference value.
    auto world = dc.to_world({128.0, 128.0}); // 0-based crpix
    assert(near(world[0], 180.0 * kDeg2Rad, 1e-4));
    assert(near(world[1], 45.0 * kDeg2Rad, 1e-4));

    return true;
}

bool test_galactic_projection() {
    Record fits;
    fits.set("NAXIS", RecordValue(std::int32_t(2)));
    fits.set("CTYPE1", RecordValue(std::string("GLON-TAN")));
    fits.set("CTYPE2", RecordValue(std::string("GLAT-TAN")));
    fits.set("CRVAL1", RecordValue(120.0));
    fits.set("CRVAL2", RecordValue(-5.0));
    fits.set("CRPIX1", RecordValue(65.0));
    fits.set("CRPIX2", RecordValue(65.0));
    fits.set("CDELT1", RecordValue(-0.01));
    fits.set("CDELT2", RecordValue(0.01));

    CoordinateSystem cs = coordinate_system_from_fits_header(fits);
    [[maybe_unused]] auto idx = find_direction_coordinate(cs);
    assert(idx.has_value());
    [[maybe_unused]] const auto& dc =
        static_cast<const DirectionCoordinate&>(cs.coordinate(*idx));
    assert(dc.ref_frame() == DirectionRef::galactic);
    assert(dc.projection().type == ProjectionType::tan);
    return true;
}

bool test_roundtrip() {
    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}}, 0.0, 0.0,
        -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 50.0, 50.0));
    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    Record fits = coordinate_system_to_fits_header(cs);

    // Check that FITS keywords exist.
    [[maybe_unused]] const auto* ct1 = fits.find("CTYPE1");
    assert(ct1 != nullptr);
    [[maybe_unused]] const auto* ct2 = fits.find("CTYPE2");
    assert(ct2 != nullptr);
    [[maybe_unused]] const auto* ct3 = fits.find("CTYPE3");
    assert(ct3 != nullptr);

    return true;
}

} // namespace

int main() {
    int failures = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "  " << name << " ... ";
        try {
            if (fn()) {
                std::cout << "PASS\n";
            } else {
                std::cout << "FAIL\n";
                ++failures;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << "\n";
            ++failures;
        }
    };

    std::cout << "fits_coordinate_util_test\n";
    run("ra_dec_freq_stokes", test_ra_dec_freq_stokes);
    run("galactic_projection", test_galactic_projection);
    run("roundtrip", test_roundtrip);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
