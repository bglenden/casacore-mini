#include "casacore_mini/erfa_bridge.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

// ERFA is a C library.
extern "C" {
#include <erfa.h>
}

namespace casacore_mini {

namespace {
void check_erfa(int status, const char* fn) {
    // ERFA uses negative codes for errors, positive for warnings.
    if (status < 0) {
        throw std::runtime_error(std::string("ERFA ") + fn + " failed with status " +
                                 std::to_string(status));
    }
}
} // namespace

void utc_to_tai(double utc1, double utc2, double& tai1, double& tai2) {
    check_erfa(eraUtctai(utc1, utc2, &tai1, &tai2), "eraUtctai");
}

void tai_to_utc(double tai1, double tai2, double& utc1, double& utc2) {
    check_erfa(eraTaiutc(tai1, tai2, &utc1, &utc2), "eraTaiutc");
}

void tai_to_tt(double tai1, double tai2, double& tt1, double& tt2) {
    check_erfa(eraTaitt(tai1, tai2, &tt1, &tt2), "eraTaitt");
}

void tt_to_tai(double tt1, double tt2, double& tai1, double& tai2) {
    check_erfa(eraTttai(tt1, tt2, &tai1, &tai2), "eraTttai");
}

void utc_to_ut1(double utc1, double utc2, double dut1, double& ut11, double& ut12) {
    check_erfa(eraUtcut1(utc1, utc2, dut1, &ut11, &ut12), "eraUtcut1");
}

void ut1_to_utc(double ut11, double ut12, double dut1, double& utc1, double& utc2) {
    check_erfa(eraUt1utc(ut11, ut12, dut1, &utc1, &utc2), "eraUt1utc");
}

void tt_to_tdb(double tt1, double tt2, double dtr, double& tdb1, double& tdb2) {
    check_erfa(eraTttdb(tt1, tt2, dtr, &tdb1, &tdb2), "eraTttdb");
}

void tdb_to_tt(double tdb1, double tdb2, double dtr, double& tt1, double& tt2) {
    check_erfa(eraTdbtt(tdb1, tdb2, dtr, &tt1, &tt2), "eraTdbtt");
}

double approx_dtdb(double tt1, double tt2) {
    // Approximate TDB-TT at geocenter: ut ~= TT, elong=0, u=0, v=0.
    double ut = std::fmod(tt1, 1.0) + std::fmod(tt2, 1.0);
    return eraDtdb(tt1, tt2, ut, 0.0, 0.0, 0.0);
}

double jd_to_besselian_epoch(double jd1, double jd2) {
    return eraEpb(jd1, jd2);
}

void wgs84_to_itrf(double elong, double phi, double height, double (&xyz)[3]) {
    check_erfa(eraGd2gc(1, elong, phi, height, xyz), "eraGd2gc");
}

void itrf_to_wgs84(const double (&xyz)[3], double& elong, double& phi, double& height) {
    // eraGc2gd takes a non-const double[3] — cast away const safely.
    check_erfa(eraGc2gd(1, const_cast<double*>(xyz), &elong, &phi, &height), // NOLINT
               "eraGc2gd");
}

void j2000_to_galactic(double ra, double dec, double& l, double& b) {
    eraIcrs2g(ra, dec, &l, &b);
}

void galactic_to_j2000(double l, double b, double& ra, double& dec) {
    eraG2icrs(l, b, &ra, &dec);
}

void b1950_to_j2000(double ra_b1950, double dec_b1950, double bepoch, double& ra_j2000,
                    double& dec_j2000) {
    eraFk45z(ra_b1950, dec_b1950, bepoch, &ra_j2000, &dec_j2000);
}

void j2000_to_b1950(double ra_j2000, double dec_j2000, double bepoch, double& ra_b1950,
                    double& dec_b1950) {
    double dra = 0.0;
    double ddec = 0.0;
    eraFk54z(ra_j2000, dec_j2000, bepoch, &ra_b1950, &dec_b1950, &dra, &ddec);
}

void bias_precession_nutation_matrix(double tt1, double tt2,
                                     std::array<std::array<double, 3>, 3>& rbpn) {
    // ERFA uses double[3][3].
    double mat[3][3]; // NOLINT
    eraPnm06a(tt1, tt2, mat);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            rbpn[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = mat[i][j];
        }
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void rotate_direction(const std::array<std::array<double, 3>, 3>& mat, double ra_in, double dec_in,
                      double& ra_out, double& dec_out) {
    // Spherical -> Cartesian.
    double cd = std::cos(dec_in);
    double x = std::cos(ra_in) * cd;
    double y = std::sin(ra_in) * cd;
    double z = std::sin(dec_in);

    // Matrix multiply.
    double xr = mat[0][0] * x + mat[0][1] * y + mat[0][2] * z;
    double yr = mat[1][0] * x + mat[1][1] * y + mat[1][2] * z;
    double zr = mat[2][0] * x + mat[2][1] * y + mat[2][2] * z;

    // Cartesian -> Spherical.
    ra_out = std::atan2(yr, xr);
    if (ra_out < 0.0) {
        ra_out += 2.0 * M_PI;
    }
    dec_out = std::atan2(zr, std::sqrt(xr * xr + yr * yr));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void rotate_direction_inverse(const std::array<std::array<double, 3>, 3>& mat, double ra_in,
                              double dec_in, double& ra_out, double& dec_out) {
    // Transpose the matrix.
    std::array<std::array<double, 3>, 3> mt{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            mt[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
                mat[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)];
        }
    }
    rotate_direction(mt, ra_in, dec_in, ra_out, dec_out);
}

double mean_obliquity(double tt1, double tt2) {
    return eraObl06(tt1, tt2);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void j2000_to_ecliptic(double ra, double dec, double& elon, double& elat) {
    // Mean obliquity at J2000.0 (TT = 2451545.0, 0.0).
    double eps = eraObl06(2451545.0, 0.0);

    // Rotation about X-axis by obliquity.
    double cd = std::cos(dec);
    double x = std::cos(ra) * cd;
    double y = std::sin(ra) * cd;
    double z = std::sin(dec);

    double ce = std::cos(eps);
    double se = std::sin(eps);

    double xr = x;
    double yr = ce * y + se * z;
    double zr = -se * y + ce * z;

    elon = std::atan2(yr, xr);
    if (elon < 0.0) {
        elon += 2.0 * M_PI;
    }
    elat = std::atan2(zr, std::sqrt(xr * xr + yr * yr));
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ecliptic_to_j2000(double elon, double elat, double& ra, double& dec) {
    double eps = eraObl06(2451545.0, 0.0);

    double cd = std::cos(elat);
    double x = std::cos(elon) * cd;
    double y = std::sin(elon) * cd;
    double z = std::sin(elat);

    double ce = std::cos(eps);
    double se = std::sin(eps);

    // Inverse rotation (negative angle).
    double xr = x;
    double yr = ce * y - se * z;
    double zr = se * y + ce * z;

    ra = std::atan2(yr, xr);
    if (ra < 0.0) {
        ra += 2.0 * M_PI;
    }
    dec = std::atan2(zr, std::sqrt(xr * xr + yr * yr));
}

} // namespace casacore_mini
