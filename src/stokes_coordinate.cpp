// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/stokes_coordinate.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace casacore_mini {

StokesCoordinate::StokesCoordinate(std::vector<std::int32_t> stokes_values)
    : stokes_(std::move(stokes_values)) {
    if (stokes_.empty()) {
        throw std::invalid_argument("StokesCoordinate requires at least one Stokes value");
    }
}

std::vector<std::string> StokesCoordinate::world_axis_names() const {
    return {"Stokes"};
}
std::vector<std::string> StokesCoordinate::world_axis_units() const {
    return {""};
}
std::vector<double> StokesCoordinate::reference_value() const {
    return {static_cast<double>(stokes_[0])};
}
std::vector<double> StokesCoordinate::reference_pixel() const {
    return {0.0};
}
std::vector<double> StokesCoordinate::increment() const {
    return {1.0};
}

std::vector<double> StokesCoordinate::to_world(const std::vector<double>& pixel) const {
    if (pixel.size() != 1) {
        throw std::invalid_argument("StokesCoordinate::to_world: expected 1 pixel axis");
    }
    auto idx = static_cast<std::size_t>(std::round(pixel[0]));
    if (idx >= stokes_.size()) {
        throw std::invalid_argument("StokesCoordinate::to_world: pixel index out of range");
    }
    return {static_cast<double>(stokes_[idx])};
}

std::vector<double> StokesCoordinate::to_pixel(const std::vector<double>& world) const {
    if (world.size() != 1) {
        throw std::invalid_argument("StokesCoordinate::to_pixel: expected 1 world axis");
    }
    auto code = static_cast<std::int32_t>(std::round(world[0]));
    for (std::size_t i = 0; i < stokes_.size(); ++i) {
        if (stokes_[i] == code) {
            return {static_cast<double>(i)};
        }
    }
    throw std::invalid_argument("StokesCoordinate::to_pixel: Stokes code not found");
}

Record StokesCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("stokes")));
    std::vector<std::int32_t> vals = stokes_;
    rec.set("stokes", RecordValue(RecordValue::int32_array{
                          {static_cast<std::uint64_t>(vals.size())}, std::move(vals)}));
    return rec;
}

std::unique_ptr<Coordinate> StokesCoordinate::clone() const {
    return std::make_unique<StokesCoordinate>(stokes_);
}

std::unique_ptr<StokesCoordinate> StokesCoordinate::from_record(const Record& rec) {
    const auto* val = rec.find("stokes");
    if (val == nullptr) {
        throw std::invalid_argument("StokesCoordinate::from_record: missing 'stokes' field");
    }
    const auto* arr = std::get_if<RecordValue::int32_array>(&val->storage());
    if (arr == nullptr) {
        throw std::invalid_argument("StokesCoordinate::from_record: 'stokes' not int32 array");
    }
    return std::make_unique<StokesCoordinate>(arr->elements);
}

} // namespace casacore_mini
