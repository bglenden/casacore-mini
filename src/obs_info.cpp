#include "casacore_mini/obs_info.hpp"
#include "casacore_mini/measure_record.hpp"

#include <stdexcept>
#include <string>

namespace casacore_mini {

namespace {

const std::string* find_string(const Record& rec, std::string_view key) {
    const auto* val = rec.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    return std::get_if<std::string>(&val->storage());
}

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

} // namespace

void obs_info_to_record(const ObsInfo& info, Record& rec) {
    if (!info.telescope.empty()) {
        rec.set("telescope", RecordValue(info.telescope));
    }
    if (!info.observer.empty()) {
        rec.set("observer", RecordValue(info.observer));
    }
    if (info.obs_date.has_value()) {
        rec.set("obsdate", RecordValue::from_record(measure_to_record(*info.obs_date)));
    }
    if (info.telescope_position.has_value()) {
        rec.set("telescopeposition",
                RecordValue::from_record(measure_to_record(*info.telescope_position)));
    }
    if (info.pointing_center.has_value()) {
        RecordValue::double_array arr;
        arr.shape = {2};
        arr.elements = {info.pointing_center->first, info.pointing_center->second};
        Record pc_rec;
        pc_rec.set("value", RecordValue(std::move(arr)));
        rec.set("pointingcenter", RecordValue::from_record(std::move(pc_rec)));
    }
}

ObsInfo obs_info_from_record(const Record& rec) {
    ObsInfo info;

    const auto* tel = find_string(rec, "telescope");
    if (tel != nullptr) {
        info.telescope = *tel;
    }

    const auto* obs = find_string(rec, "observer");
    if (obs != nullptr) {
        info.observer = *obs;
    }

    const auto* date_rec = find_sub_record(rec, "obsdate");
    if (date_rec != nullptr) {
        try {
            info.obs_date = measure_from_record(*date_rec);
        } catch (const std::exception&) {
            // Upstream casacore may write measure records in a format we
            // don't fully support yet.  Skip gracefully.
        }
    }

    const auto* pos_rec = find_sub_record(rec, "telescopeposition");
    if (pos_rec != nullptr) {
        try {
            info.telescope_position = measure_from_record(*pos_rec);
        } catch (const std::exception&) {
            // Gracefully skip unsupported measure format.
        }
    }

    const auto* pc_rec = find_sub_record(rec, "pointingcenter");
    if (pc_rec != nullptr) {
        const auto* val = pc_rec->find("value");
        if (val != nullptr) {
            if (const auto* arr = std::get_if<RecordValue::double_array>(&val->storage())) {
                if (arr->elements.size() >= 2) {
                    info.pointing_center = {arr->elements[0], arr->elements[1]};
                }
            }
        }
    }

    return info;
}

} // namespace casacore_mini
