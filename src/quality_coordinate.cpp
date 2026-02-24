#include "casacore_mini/quality_coordinate.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace casacore_mini {

QualityCoordinate::QualityCoordinate(std::vector<std::int32_t> quality_values)
    : quality_(std::move(quality_values)) {
    if (quality_.empty()) {
        throw std::invalid_argument("QualityCoordinate requires at least one value");
    }
}

std::vector<std::string> QualityCoordinate::world_axis_names() const {
    return {"Quality"};
}
std::vector<std::string> QualityCoordinate::world_axis_units() const {
    return {""};
}
std::vector<double> QualityCoordinate::reference_value() const {
    return {static_cast<double>(quality_[0])};
}
std::vector<double> QualityCoordinate::reference_pixel() const {
    return {0.0};
}
std::vector<double> QualityCoordinate::increment() const {
    return {1.0};
}

std::vector<double> QualityCoordinate::to_world(const std::vector<double>& pixel) const {
    if (pixel.size() != 1) {
        throw std::invalid_argument("QualityCoordinate::to_world: expected 1 pixel axis");
    }
    auto idx = static_cast<std::size_t>(std::round(pixel[0]));
    if (idx >= quality_.size()) {
        throw std::invalid_argument("QualityCoordinate::to_world: pixel index out of range");
    }
    return {static_cast<double>(quality_[idx])};
}

std::vector<double> QualityCoordinate::to_pixel(const std::vector<double>& world) const {
    if (world.size() != 1) {
        throw std::invalid_argument("QualityCoordinate::to_pixel: expected 1 world axis");
    }
    auto code = static_cast<std::int32_t>(std::round(world[0]));
    for (std::size_t i = 0; i < quality_.size(); ++i) {
        if (quality_[i] == code) {
            return {static_cast<double>(i)};
        }
    }
    throw std::invalid_argument("QualityCoordinate::to_pixel: quality code not found");
}

Record QualityCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("quality")));
    std::vector<std::int32_t> vals = quality_;
    rec.set("quality", RecordValue(RecordValue::int32_array{
                           {static_cast<std::uint64_t>(vals.size())}, std::move(vals)}));
    return rec;
}

std::unique_ptr<Coordinate> QualityCoordinate::clone() const {
    return std::make_unique<QualityCoordinate>(quality_);
}

std::unique_ptr<QualityCoordinate> QualityCoordinate::from_record(const Record& rec) {
    const auto* val = rec.find("quality");
    if (val == nullptr) {
        throw std::invalid_argument("QualityCoordinate::from_record: missing 'quality' field");
    }
    const auto* arr = std::get_if<RecordValue::int32_array>(&val->storage());
    if (arr == nullptr) {
        throw std::invalid_argument("QualityCoordinate::from_record: 'quality' not int32 array");
    }
    return std::make_unique<QualityCoordinate>(arr->elements);
}

} // namespace casacore_mini
