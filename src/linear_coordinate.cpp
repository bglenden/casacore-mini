#include "casacore_mini/linear_coordinate.hpp"

#include <stdexcept>
#include <string>

namespace casacore_mini {

LinearCoordinate::LinearCoordinate(std::vector<std::string> names, std::vector<std::string> units,
                                   LinearXform xform)
    : names_(std::move(names)), units_(std::move(units)), xform_(std::move(xform)) {}

std::vector<double> LinearCoordinate::to_world(const std::vector<double>& pixel) const {
    return xform_.pixel_to_world(pixel);
}

std::vector<double> LinearCoordinate::to_pixel(const std::vector<double>& world) const {
    return xform_.world_to_pixel(world);
}

Record LinearCoordinate::save() const {
    Record rec;
    rec.set("coordinate_type", RecordValue(std::string("linear")));

    rec.set("crval", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(xform_.crval.size())}, xform_.crval}));
    rec.set("crpix", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(xform_.crpix.size())}, xform_.crpix}));
    rec.set("cdelt", RecordValue(RecordValue::double_array{
                         {static_cast<std::uint64_t>(xform_.cdelt.size())}, xform_.cdelt}));

    // PC matrix — casacore requires this field as a 2D (n×n) array.
    // Generate identity if absent.
    {
        const auto n = static_cast<std::uint64_t>(xform_.crval.size());
        std::vector<double> pc_data;
        if (!xform_.pc.empty()) {
            pc_data = xform_.pc;
        } else {
            pc_data.resize(n * n, 0.0);
            for (std::uint64_t i = 0; i < n; ++i) {
                pc_data[i * n + i] = 1.0;
            }
        }
        rec.set("pc", RecordValue(RecordValue::double_array{{n, n}, std::move(pc_data)}));
    }

    // casacore uses "axes" (not "names") for world axis names.
    rec.set("axes", RecordValue(RecordValue::string_array{
                        {static_cast<std::uint64_t>(names_.size())}, names_}));
    rec.set("units", RecordValue(RecordValue::string_array{
                         {static_cast<std::uint64_t>(units_.size())}, units_}));
    return rec;
}

std::unique_ptr<Coordinate> LinearCoordinate::clone() const {
    return std::make_unique<LinearCoordinate>(names_, units_, xform_);
}

std::unique_ptr<LinearCoordinate> LinearCoordinate::from_record(const Record& rec) {
    auto get_strings = [&](const char* key) -> std::vector<std::string> {
        const auto* v = rec.find(key);
        if (v == nullptr) {
            return {};
        }
        if (const auto* a = std::get_if<RecordValue::string_array>(&v->storage())) {
            return a->elements;
        }
        return {};
    };
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

    LinearXform xf;
    xf.crval = get_doubles("crval");
    xf.crpix = get_doubles("crpix");
    xf.cdelt = get_doubles("cdelt");
    xf.pc = get_doubles("pc");

    // casacore uses "axes"; accept "names" as fallback for older mini images.
    auto axis_names = get_strings("axes");
    if (axis_names.empty()) {
        axis_names = get_strings("names");
    }
    return std::make_unique<LinearCoordinate>(std::move(axis_names), get_strings("units"),
                                              std::move(xf));
}

} // namespace casacore_mini
