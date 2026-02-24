#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/obs_info.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

const Record* find_sub_record(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    const auto* rp = std::get_if<RecordValue::record_ptr>(&val->storage());
    if (rp == nullptr || *rp == nullptr) {
        return nullptr;
    }
    return rp->get();
}

std::vector<double> get_double_array(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<RecordValue::double_array>(&val->storage())) {
        return arr->elements;
    }
    return {};
}

std::vector<std::int64_t> get_int64_array(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<RecordValue::int64_array>(&val->storage())) {
        return arr->elements;
    }
    // May be stored as int32 array.
    if (const auto* arr = std::get_if<RecordValue::int32_array>(&val->storage())) {
        std::vector<std::int64_t> result;
        result.reserve(arr->elements.size());
        for (auto v : arr->elements) {
            result.push_back(static_cast<std::int64_t>(v));
        }
        return result;
    }
    return {};
}

// Coordinate type string prefix for Record key naming (e.g. "direction", "spectral").
std::string coord_key_prefix(CoordinateType t) {
    return coordinate_type_to_string(t);
}

} // namespace

std::size_t CoordinateSystem::add_coordinate(std::unique_ptr<Coordinate> coord) {
    std::size_t idx = coords_.size();

    CoordEntry entry;
    std::size_t nw = coord->n_world_axes();
    std::size_t np = coord->n_pixel_axes();

    entry.world_map.resize(nw);
    entry.pixel_map.resize(np);
    entry.world_replace.resize(nw, 0.0);
    entry.pixel_replace.resize(np, 0.0);

    constexpr auto kMaxAxisIndex =
        static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
    if (nw > (kMaxAxisIndex - total_world_) || np > (kMaxAxisIndex - total_pixel_)) {
        throw std::overflow_error("CoordinateSystem axis count exceeds int64 mapping capacity");
    }
    auto world_base = static_cast<std::int64_t>(total_world_);
    auto pixel_base = static_cast<std::int64_t>(total_pixel_);

    for (std::size_t i = 0; i < nw; ++i) {
        entry.world_map[i] = world_base + static_cast<std::int64_t>(i);
    }
    for (std::size_t i = 0; i < np; ++i) {
        entry.pixel_map[i] = pixel_base + static_cast<std::int64_t>(i);
    }

    total_world_ += nw;
    total_pixel_ += np;

    entry.coord = std::move(coord);
    coords_.push_back(std::move(entry));
    return idx;
}

const Coordinate& CoordinateSystem::coordinate(std::size_t index) const {
    if (index >= coords_.size()) {
        throw std::out_of_range("CoordinateSystem::coordinate: index out of range");
    }
    return *coords_[index].coord;
}

std::optional<std::pair<std::size_t, std::size_t>>
CoordinateSystem::find_world_axis(std::size_t world_axis) const {
    auto target = static_cast<std::int64_t>(world_axis);
    for (std::size_t ci = 0; ci < coords_.size(); ++ci) {
        const auto& wm = coords_[ci].world_map;
        for (std::size_t ai = 0; ai < wm.size(); ++ai) {
            if (wm[ai] == target) {
                return std::pair{ci, ai};
            }
        }
    }
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>>
CoordinateSystem::find_pixel_axis(std::size_t pixel_axis) const {
    auto target = static_cast<std::int64_t>(pixel_axis);
    for (std::size_t ci = 0; ci < coords_.size(); ++ci) {
        const auto& pm = coords_[ci].pixel_map;
        for (std::size_t ai = 0; ai < pm.size(); ++ai) {
            if (pm[ai] == target) {
                return std::pair{ci, ai};
            }
        }
    }
    return std::nullopt;
}

Record CoordinateSystem::save() const {
    Record rec;

    // ObsInfo at top level.
    obs_info_to_record(obs_info_, rec);

    // Per-coordinate sub-records.
    for (std::size_t i = 0; i < coords_.size(); ++i) {
        const auto& entry = coords_[i];
        std::string prefix = coord_key_prefix(entry.coord->type()) + std::to_string(i);

        rec.set(prefix, RecordValue::from_record(entry.coord->save()));

        // Axis maps.
        auto to_int64_arr = [](const std::vector<std::int64_t>& v) -> RecordValue {
            return RecordValue(RecordValue::int64_array{{static_cast<std::uint64_t>(v.size())}, v});
        };
        auto to_double_arr = [](const std::vector<double>& v) -> RecordValue {
            return RecordValue(
                RecordValue::double_array{{static_cast<std::uint64_t>(v.size())}, v});
        };

        rec.set("worldmap" + std::to_string(i), to_int64_arr(entry.world_map));
        rec.set("pixelmap" + std::to_string(i), to_int64_arr(entry.pixel_map));
        rec.set("worldreplace" + std::to_string(i), to_double_arr(entry.world_replace));
        rec.set("pixelreplace" + std::to_string(i), to_double_arr(entry.pixel_replace));
    }

    return rec;
}

CoordinateSystem CoordinateSystem::restore(const Record& rec) {
    CoordinateSystem cs;

    // Restore ObsInfo.
    cs.obs_info_ = obs_info_from_record(rec);

    // Scan entries for coordinate sub-records.
    // Keys are like "direction0", "spectral1", "stokes2", etc.
    // We iterate numerically from 0 until we stop finding coordinates.
    for (std::size_t i = 0;; ++i) {
        // Try all known type prefixes.
        const Record* coord_rec = nullptr;
        std::string found_key;

        static const char* prefixes[] = {"direction", "spectral", "stokes",
                                         "linear",    "tabular",  "quality"};
        for (const auto* pfx : prefixes) {
            std::string key = std::string(pfx) + std::to_string(i);
            coord_rec = find_sub_record(rec, key);
            if (coord_rec != nullptr) {
                found_key = key;
                break;
            }
        }

        if (coord_rec == nullptr) {
            break; // No more coordinates.
        }

        auto coord = Coordinate::restore(*coord_rec);
        std::size_t idx = cs.add_coordinate(std::move(coord));

        // Override axis maps if present.
        auto wmap = get_int64_array(rec, "worldmap" + std::to_string(i));
        if (!wmap.empty()) {
            cs.coords_[idx].world_map = std::move(wmap);
        }
        auto pmap = get_int64_array(rec, "pixelmap" + std::to_string(i));
        if (!pmap.empty()) {
            cs.coords_[idx].pixel_map = std::move(pmap);
        }
        auto wreplace = get_double_array(rec, "worldreplace" + std::to_string(i));
        if (!wreplace.empty()) {
            cs.coords_[idx].world_replace = std::move(wreplace);
        }
        auto preplace = get_double_array(rec, "pixelreplace" + std::to_string(i));
        if (!preplace.empty()) {
            cs.coords_[idx].pixel_replace = std::move(preplace);
        }
    }

    return cs;
}

} // namespace casacore_mini
