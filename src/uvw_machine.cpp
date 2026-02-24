#include "casacore_mini/uvw_machine.hpp"

#include <cmath>
#include <stdexcept>

extern "C" {
#include <erfa.h>
}

namespace casacore_mini {

namespace {

// Standard UVW rotation matrix for a phase center at (ra, dec).
// Maps XYZ baselines → UVW coordinates.
//   T(ra,dec) = Ry(-dec) * Rz(ra)
// Rows: u along East, v toward NCP, w toward source.
using mat3 = std::array<std::array<double, 3>, 3>;

mat3 uvw_frame_matrix(double ra, double dec) {
    double cr = std::cos(ra);
    double sr = std::sin(ra);
    double cd = std::cos(dec);
    double sd = std::sin(dec);

    // Row 0 (u): pointing East
    // Row 1 (v): pointing toward NCP projected
    // Row 2 (w): pointing toward source
    return {{
        {{sr, -cr, 0.0}},
        {{-sd * cr, -sd * sr, cd}},
        {{cd * cr, cd * sr, sd}},
    }};
}

// 3x3 matrix multiply: C = A * B.
mat3 mat_mul(const mat3& a, const mat3& b) {
    mat3 c{};
    for (int i = 0; i < 3; ++i) {
        auto ii = static_cast<std::size_t>(i);
        for (int j = 0; j < 3; ++j) {
            auto jj = static_cast<std::size_t>(j);
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) {
                auto kk = static_cast<std::size_t>(k);
                sum += a[ii][kk] * b[kk][jj];
            }
            c[ii][jj] = sum;
        }
    }
    return c;
}

// 3x3 matrix transpose.
mat3 mat_transpose(const mat3& m) {
    mat3 t{};
    for (int i = 0; i < 3; ++i) {
        auto ii = static_cast<std::size_t>(i);
        for (int j = 0; j < 3; ++j) {
            auto jj = static_cast<std::size_t>(j);
            t[ii][jj] = m[jj][ii];
        }
    }
    return t;
}

} // namespace

std::array<std::array<double, 3>, 3> uvw_rotation_matrix(double ra1, double dec1, double ra2,
                                                         double dec2) {
    // The rotation from UVW frame 1 to UVW frame 2 is:
    //   R = T2 * T1^T
    // where T(ra,dec) is the standard UVW frame matrix.
    // For same phase center: R = T * T^T = I (orthogonal matrix).
    auto t1 = uvw_frame_matrix(ra1, dec1);
    auto t2 = uvw_frame_matrix(ra2, dec2);
    return mat_mul(t2, mat_transpose(t1));
}

UvwValue rotate_uvw(const std::array<std::array<double, 3>, 3>& matrix, const UvwValue& uvw) {
    return UvwValue{matrix[0][0] * uvw.u_m + matrix[0][1] * uvw.v_m + matrix[0][2] * uvw.w_m,
                    matrix[1][0] * uvw.u_m + matrix[1][1] * uvw.v_m + matrix[1][2] * uvw.w_m,
                    matrix[2][0] * uvw.u_m + matrix[2][1] * uvw.v_m + matrix[2][2] * uvw.w_m};
}

double parallactic_angle(double ha_rad, double dec_rad, double lat_rad) {
    return eraHd2pa(ha_rad, dec_rad, lat_rad);
}

void earth_magnetic_field(double /*lon_rad*/, double /*lat_rad*/, double /*height_m*/,
                          double /*epoch_yr*/) {
    throw std::runtime_error("IGRF not implemented");
}

UvwMachine::UvwMachine(double from_ra_rad, double from_dec_rad, double to_ra_rad, double to_dec_rad)
    : rotation_matrix_(uvw_rotation_matrix(from_ra_rad, from_dec_rad, to_ra_rad, to_dec_rad)) {}

UvwValue UvwMachine::convert(const UvwValue& uvw) const {
    return rotate_uvw(rotation_matrix_, uvw);
}

double ParAngleMachine::compute(double hour_angle_rad, double dec_rad) const {
    return parallactic_angle(hour_angle_rad, dec_rad, latitude_rad_);
}

void EarthMagneticMachine::compute(double lon_rad, double lat_rad, double height_m) const {
    earth_magnetic_field(lon_rad, lat_rad, height_m, epoch_yr_);
}

} // namespace casacore_mini
