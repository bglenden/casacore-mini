// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/gaussian_convert.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace casacore_mini {

namespace {

// Compute the Jacobian of the world-to-pixel transform at a given pixel position.
// Returns [dx/dlon, dx/dlat, dy/dlon, dy/dlat] (row-major 2x2).
void compute_jacobian(const DirectionCoordinate& coord, const std::vector<double>& pixel,
                      double jac[4]) {
    constexpr double kEps = 1.0e-6; // 1 arcsec in radians, roughly

    auto world0 = coord.to_world(pixel);

    // Perturb lon.
    auto world_dlon = world0;
    world_dlon[0] += kEps;
    auto pix_dlon = coord.to_pixel(world_dlon);

    // Perturb lat.
    auto world_dlat = world0;
    world_dlat[1] += kEps;
    auto pix_dlat = coord.to_pixel(world_dlat);

    // J = d(pixel)/d(world)
    jac[0] = (pix_dlon[0] - pixel[0]) / kEps; // dx/dlon
    jac[1] = (pix_dlat[0] - pixel[0]) / kEps; // dx/dlat
    jac[2] = (pix_dlon[1] - pixel[1]) / kEps; // dy/dlon
    jac[3] = (pix_dlat[1] - pixel[1]) / kEps; // dy/dlat
}

// Transform a Gaussian shape using a 2x2 Jacobian matrix.
// Input: (major, minor, pa) in source coordinate system.
// Jacobian: transforms from source coords to target coords.
// Returns: (major, minor, pa) in target coordinate system.
std::tuple<double, double, double> transform_gaussian(double major, double minor, double pa,
                                                      const double jac[4]) {
    // Build the covariance-like matrix for the Gaussian.
    double cos_pa = std::cos(pa);
    double sin_pa = std::sin(pa);

    // Gaussian shape matrix S = R * diag(major^2, minor^2) * R^T
    // where R is rotation by pa.
    double a = major * major;
    double b = minor * minor;
    double s00 = a * cos_pa * cos_pa + b * sin_pa * sin_pa;
    double s01 = (a - b) * cos_pa * sin_pa;
    double s11 = a * sin_pa * sin_pa + b * cos_pa * cos_pa;

    // Transform: S' = J * S * J^T
    double j00 = jac[0], j01 = jac[1], j10 = jac[2], j11 = jac[3];
    double t00 = j00 * s00 + j01 * s01;
    double t01 = j00 * s01 + j01 * s11;
    double t10 = j10 * s00 + j11 * s01;
    double t11 = j10 * s01 + j11 * s11;

    double sp00 = t00 * j00 + t01 * j01;
    double sp01 = t00 * j10 + t01 * j11;
    double sp11 = t10 * j10 + t11 * j11;

    // Extract major, minor, pa from S'.
    double trace = sp00 + sp11;
    double det = sp00 * sp11 - sp01 * sp01;
    double discriminant = trace * trace - 4.0 * det;
    if (discriminant < 0.0) {
        discriminant = 0.0;
    }
    double sqrt_disc = std::sqrt(discriminant);

    double lambda1 = 0.5 * (trace + sqrt_disc);
    double lambda2 = 0.5 * (trace - sqrt_disc);
    if (lambda1 < 0.0) {
        lambda1 = 0.0;
    }
    if (lambda2 < 0.0) {
        lambda2 = 0.0;
    }

    double new_major = std::sqrt(lambda1);
    double new_minor = std::sqrt(lambda2);
    double new_pa = 0.5 * std::atan2(2.0 * sp01, sp00 - sp11);

    return {new_major, new_minor, new_pa};
}

} // namespace

std::tuple<double, double, double> gaussian_world_to_pixel(double major_rad, double minor_rad,
                                                           double pa_rad,
                                                           const DirectionCoordinate& coord,
                                                           const std::vector<double>& pixel) {
    double jac[4]; // NOLINT(modernize-avoid-c-arrays)
    compute_jacobian(coord, pixel, jac);
    return transform_gaussian(major_rad, minor_rad, pa_rad, jac);
}

std::tuple<double, double, double> gaussian_pixel_to_world(double major_pix, double minor_pix,
                                                           double pa_pix,
                                                           const DirectionCoordinate& coord,
                                                           const std::vector<double>& pixel) {
    // For pixel-to-world, we need the inverse Jacobian.
    double jac[4]; // NOLINT(modernize-avoid-c-arrays)
    compute_jacobian(coord, pixel, jac);

    // Invert the 2x2 Jacobian.
    double det = jac[0] * jac[3] - jac[1] * jac[2];
    if (std::abs(det) < 1.0e-30) {
        throw std::invalid_argument("Singular Jacobian in gaussian_pixel_to_world");
    }
    double inv[4]; // NOLINT(modernize-avoid-c-arrays)
    inv[0] = jac[3] / det;
    inv[1] = -jac[1] / det;
    inv[2] = -jac[2] / det;
    inv[3] = jac[0] / det;

    return transform_gaussian(major_pix, minor_pix, pa_pix, inv);
}

} // namespace casacore_mini
