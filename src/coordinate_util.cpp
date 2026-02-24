#include "casacore_mini/coordinate_util.hpp"

namespace casacore_mini {

std::optional<std::size_t> find_direction_coordinate(const CoordinateSystem& cs) {
    for (std::size_t i = 0; i < cs.n_coordinates(); ++i) {
        if (cs.coordinate(i).type() == CoordinateType::direction) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> find_spectral_coordinate(const CoordinateSystem& cs) {
    for (std::size_t i = 0; i < cs.n_coordinates(); ++i) {
        if (cs.coordinate(i).type() == CoordinateType::spectral) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> find_stokes_coordinate(const CoordinateSystem& cs) {
    for (std::size_t i = 0; i < cs.n_coordinates(); ++i) {
        if (cs.coordinate(i).type() == CoordinateType::stokes) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace casacore_mini
