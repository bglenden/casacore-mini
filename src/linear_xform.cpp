#include "casacore_mini/linear_xform.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace casacore_mini {

namespace {

// Get pc matrix element, defaulting to identity if pc is empty.
double get_pc(const std::vector<double>& pc, std::size_t n, std::size_t i, std::size_t j) {
    if (pc.empty()) {
        return (i == j) ? 1.0 : 0.0;
    }
    return pc[i * n + j];
}

// Invert an n x n matrix in-place using Gauss-Jordan elimination.
// Returns false if singular.
bool invert_matrix(std::vector<double>& m, std::size_t n) {
    // Augment with identity.
    std::vector<double> aug(n * 2 * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            aug[i * 2 * n + j] = m[i * n + j];
        }
        aug[i * 2 * n + n + i] = 1.0;
    }

    for (std::size_t col = 0; col < n; ++col) {
        // Partial pivot.
        std::size_t pivot = col;
        double best = std::abs(aug[col * 2 * n + col]);
        for (std::size_t row = col + 1; row < n; ++row) {
            double val = std::abs(aug[row * 2 * n + col]);
            if (val > best) {
                best = val;
                pivot = row;
            }
        }
        if (best < 1.0e-15) {
            return false; // singular
        }
        if (pivot != col) {
            for (std::size_t j = 0; j < 2 * n; ++j) {
                std::swap(aug[col * 2 * n + j], aug[pivot * 2 * n + j]);
            }
        }

        double diag = aug[col * 2 * n + col];
        for (std::size_t j = 0; j < 2 * n; ++j) {
            aug[col * 2 * n + j] /= diag;
        }

        for (std::size_t row = 0; row < n; ++row) {
            if (row == col) {
                continue;
            }
            double factor = aug[row * 2 * n + col];
            for (std::size_t j = 0; j < 2 * n; ++j) {
                aug[row * 2 * n + j] -= factor * aug[col * 2 * n + j];
            }
        }
    }

    // Extract inverse.
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            m[i * n + j] = aug[i * 2 * n + n + j];
        }
    }
    return true;
}

} // namespace

std::vector<double> LinearXform::pixel_to_world(const std::vector<double>& pixel) const {
    std::size_t n = n_axes();
    if (pixel.size() != n) {
        throw std::invalid_argument("pixel size " + std::to_string(pixel.size()) + " != n_axes " +
                                    std::to_string(n));
    }

    std::vector<double> world(n);
    for (std::size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            sum += cdelt[i] * get_pc(pc, n, i, j) * (pixel[j] - crpix[j]);
        }
        world[i] = crval[i] + sum;
    }
    return world;
}

std::vector<double> LinearXform::world_to_pixel(const std::vector<double>& world) const {
    std::size_t n = n_axes();
    if (world.size() != n) {
        throw std::invalid_argument("world size " + std::to_string(world.size()) + " != n_axes " +
                                    std::to_string(n));
    }

    // Build the product matrix M[i][j] = cdelt[i] * pc[i][j].
    std::vector<double> mat(n * n);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            mat[i * n + j] = cdelt[i] * get_pc(pc, n, i, j);
        }
    }

    // Invert.
    if (!invert_matrix(mat, n)) {
        throw std::invalid_argument("Singular matrix in LinearXform::world_to_pixel");
    }

    // pixel = crpix + inv(M) * (world - crval)
    std::vector<double> pixel(n);
    for (std::size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            sum += mat[i * n + j] * (world[j] - crval[j]);
        }
        pixel[i] = crpix[i] + sum;
    }
    return pixel;
}

} // namespace casacore_mini
