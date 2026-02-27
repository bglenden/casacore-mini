#include "casacore_mini/tabular_coordinate.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace casacore_mini {

TabularCoordinate::TabularCoordinate(std::vector<double> pixel_values,
                                     std::vector<double> world_values, std::string name,
                                     std::string unit)
    : pixel_values_(std::move(pixel_values)), world_values_(std::move(world_values)),
      name_(std::move(name)), unit_(std::move(unit)) {
    if (pixel_values_.size() != world_values_.size()) {
        throw std::invalid_argument("TabularCoordinate: pixel and world arrays must match");
    }
    if (pixel_values_.size() < 2) {
        throw std::invalid_argument("TabularCoordinate: need at least 2 entries");
    }
}

std::vector<std::string> TabularCoordinate::world_axis_names() const {
    return {name_};
}
std::vector<std::string> TabularCoordinate::world_axis_units() const {
    return {unit_};
}
std::vector<double> TabularCoordinate::reference_value() const {
    return {world_values_[0]};
}
std::vector<double> TabularCoordinate::reference_pixel() const {
    return {pixel_values_[0]};
}
std::vector<double> TabularCoordinate::increment() const {
    if (pixel_values_.size() >= 2) {
        double dp = pixel_values_[1] - pixel_values_[0];
        if (std::abs(dp) > 1.0e-15) {
            return {(world_values_[1] - world_values_[0]) / dp};
        }
    }
    return {1.0};
}

std::vector<double> TabularCoordinate::to_world(const std::vector<double>& pixel) const {
    if (pixel.size() != 1) {
        throw std::invalid_argument("TabularCoordinate::to_world: expected 1 pixel axis");
    }
    double p = pixel[0];

    // Find bracketing interval.
    if (p <= pixel_values_.front()) {
        // Extrapolate from first interval.
        double dp = pixel_values_[1] - pixel_values_[0];
        double dw = world_values_[1] - world_values_[0];
        double frac = (std::abs(dp) > 1e-15) ? (p - pixel_values_[0]) / dp : 0.0;
        return {world_values_[0] + frac * dw};
    }
    if (p >= pixel_values_.back()) {
        std::size_t n = pixel_values_.size();
        double dp = pixel_values_[n - 1] - pixel_values_[n - 2];
        double dw = world_values_[n - 1] - world_values_[n - 2];
        double frac = (std::abs(dp) > 1e-15) ? (p - pixel_values_[n - 2]) / dp : 0.0;
        return {world_values_[n - 2] + frac * dw};
    }

    for (std::size_t i = 0; i + 1 < pixel_values_.size(); ++i) {
        if (p >= pixel_values_[i] && p <= pixel_values_[i + 1]) {
            double dp = pixel_values_[i + 1] - pixel_values_[i];
            double frac = (std::abs(dp) > 1e-15) ? (p - pixel_values_[i]) / dp : 0.0;
            double dw = world_values_[i + 1] - world_values_[i];
            return {world_values_[i] + frac * dw};
        }
    }

    return {world_values_.back()};
}

std::vector<double> TabularCoordinate::to_pixel(const std::vector<double>& world) const {
    if (world.size() != 1) {
        throw std::invalid_argument("TabularCoordinate::to_pixel: expected 1 world axis");
    }
    double w = world[0];

    // Find bracketing interval in world values.
    // Assumes world values are monotonic (may be increasing or decreasing).
    bool increasing = world_values_.back() > world_values_.front();

    for (std::size_t i = 0; i + 1 < world_values_.size(); ++i) {
        bool in_range = increasing ? (w >= world_values_[i] && w <= world_values_[i + 1])
                                   : (w <= world_values_[i] && w >= world_values_[i + 1]);
        if (in_range) {
            double dw = world_values_[i + 1] - world_values_[i];
            double frac = (std::abs(dw) > 1e-15) ? (w - world_values_[i]) / dw : 0.0;
            double dp = pixel_values_[i + 1] - pixel_values_[i];
            return {pixel_values_[i] + frac * dp};
        }
    }

    // Extrapolate from nearest end.
    if ((increasing && w < world_values_.front()) || (!increasing && w > world_values_.front())) {
        double dw = world_values_[1] - world_values_[0];
        double dp = pixel_values_[1] - pixel_values_[0];
        double frac = (std::abs(dw) > 1e-15) ? (w - world_values_[0]) / dw : 0.0;
        return {pixel_values_[0] + frac * dp};
    }
    std::size_t n = world_values_.size();
    double dw = world_values_[n - 1] - world_values_[n - 2];
    double dp = pixel_values_[n - 1] - pixel_values_[n - 2];
    double frac = (std::abs(dw) > 1e-15) ? (w - world_values_[n - 2]) / dw : 0.0;
    return {pixel_values_[n - 2] + frac * dp};
}

Record TabularCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("tabular")));
    // Write fields in the order casacore expects.
    auto rv = reference_value();
    auto rp = reference_pixel();
    auto inc = increment();
    rec.set("crval", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(rv.size())}, rv}));
    rec.set("crpix", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(rp.size())}, rp}));
    rec.set("cdelt", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(inc.size())}, inc}));
    // PC matrix: 1×1 identity for a single-axis coordinate.
    rec.set("pc", RecordValue(RecordValue::double_array{{1, 1}, {1.0}}));
    rec.set("axes", RecordValue(RecordValue::string_array{{1}, {name_}}));
    rec.set("units", RecordValue(RecordValue::string_array{{1}, {unit_}}));
    rec.set("pixelvalues", RecordValue(RecordValue::double_array{
                               {static_cast<std::uint64_t>(pixel_values_.size())}, pixel_values_}));
    rec.set("worldvalues", RecordValue(RecordValue::double_array{
                               {static_cast<std::uint64_t>(world_values_.size())}, world_values_}));
    return rec;
}

std::unique_ptr<Coordinate> TabularCoordinate::clone() const {
    return std::make_unique<TabularCoordinate>(pixel_values_, world_values_, name_, unit_);
}

std::unique_ptr<TabularCoordinate> TabularCoordinate::from_record(const Record& rec) {
    auto get_doubles = [&](const char* key) -> std::vector<double> {
        const auto* v = rec.find(key);
        if (v == nullptr) {
            return {};
        }
        if (const auto* a = std::get_if<RecordValue::double_array>(&v->storage())) {
            return a->elements;
        }
        return {};
    };

    // Accept both casacore ("axes"/"units" arrays) and legacy mini ("name"/"unit" scalars).
    std::string name;
    if (const auto* v = rec.find("axes")) {
        if (const auto* a = std::get_if<RecordValue::string_array>(&v->storage())) {
            if (!a->elements.empty()) name = a->elements[0];
        }
    }
    if (name.empty()) {
        if (const auto* v = rec.find("name")) {
            if (const auto* sp = std::get_if<std::string>(&v->storage())) {
                name = *sp;
            }
        }
    }
    std::string unit;
    if (const auto* v = rec.find("units")) {
        if (const auto* a = std::get_if<RecordValue::string_array>(&v->storage())) {
            if (!a->elements.empty()) unit = a->elements[0];
        }
    }
    if (unit.empty()) {
        if (const auto* v = rec.find("unit")) {
            if (const auto* sp = std::get_if<std::string>(&v->storage())) {
                unit = *sp;
            }
        }
    }

    return std::make_unique<TabularCoordinate>(
        get_doubles("pixelvalues"), get_doubles("worldvalues"), std::move(name), std::move(unit));
}

} // namespace casacore_mini
