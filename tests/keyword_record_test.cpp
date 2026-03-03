// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/keyword_record.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool expect_true(const bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

const casacore_mini::KeywordRecord* as_record(const casacore_mini::KeywordValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto* storage = std::get_if<casacore_mini::KeywordValue::record_ptr>(&value->storage());
    if (storage == nullptr || !*storage) {
        return nullptr;
    }
    return storage->get();
}

const casacore_mini::KeywordArray* as_array(const casacore_mini::KeywordValue* value) {
    if (value == nullptr) {
        return nullptr;
    }
    const auto* storage = std::get_if<casacore_mini::KeywordValue::array_ptr>(&value->storage());
    if (storage == nullptr || !*storage) {
        return nullptr;
    }
    return storage->get();
}

} // namespace

int main() noexcept {
    try {
        const auto source_root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);

        const auto pagedimage_text =
            read_file(source_root / "data/corpus/fixtures/pagedimage_coords/showtableinfo.txt");
        const auto pagedimage_keywords =
            casacore_mini::parse_showtableinfo_keywords(pagedimage_text);
        const auto pagedimage_keywords_2 =
            casacore_mini::parse_showtableinfo_keywords(pagedimage_text);

        if (!expect_true(pagedimage_keywords == pagedimage_keywords_2,
                         "parsed keyword model is not deterministic/equal")) {
            return 1;
        }

        const auto* coords = as_record(pagedimage_keywords.table_keywords.find("coords"));
        if (!expect_true(coords != nullptr, "missing coords record")) {
            return 1;
        }

        const auto* direction0 = as_record(coords->find("direction0"));
        if (!expect_true(direction0 != nullptr, "missing coords.direction0 record")) {
            return 1;
        }

        const auto* system_value = direction0->find("system");
        if (!expect_true(system_value != nullptr, "missing coords.direction0.system")) {
            return 1;
        }
        const auto* as_string = std::get_if<std::string>(&system_value->storage());
        if (!expect_true(as_string != nullptr && *as_string == "J2000",
                         "coords.direction0.system value mismatch")) {
            return 1;
        }

        const auto* axes = as_array(direction0->find("axes"));
        if (!expect_true(axes != nullptr, "missing coords.direction0.axes array")) {
            return 1;
        }
        if (!expect_true(axes->elements.size() == 2U, "coords.direction0.axes size mismatch")) {
            return 1;
        }
        const auto* axis0 = std::get_if<std::string>(&axes->elements[0].storage());
        const auto* axis1 = std::get_if<std::string>(&axes->elements[1].storage());
        if (!expect_true(axis0 != nullptr && axis1 != nullptr && *axis0 == "Right Ascension" &&
                             *axis1 == "Declination",
                         "coords.direction0.axes values mismatch")) {
            return 1;
        }

        const auto* obsdate = as_record(coords->find("obsdate"));
        if (!expect_true(obsdate != nullptr, "missing coords.obsdate record")) {
            return 1;
        }
        const auto* type_value = obsdate->find("type");
        const auto* type_as_string =
            (type_value == nullptr) ? nullptr : std::get_if<std::string>(&type_value->storage());
        if (!expect_true(type_as_string != nullptr && *type_as_string == "epoch",
                         "coords.obsdate.type value mismatch")) {
            return 1;
        }

        const auto ms_text =
            read_file(source_root / "data/corpus/fixtures/ms_tree/showtableinfo.txt");
        const auto ms_keywords = casacore_mini::parse_showtableinfo_keywords(ms_text);
        const auto* uvw_column = ms_keywords.find_column("UVW");
        if (!expect_true(uvw_column != nullptr, "missing column keyword record for UVW")) {
            return 1;
        }
        const auto* measinfo = as_record(uvw_column->find("MEASINFO"));
        if (!expect_true(measinfo != nullptr, "missing UVW MEASINFO record")) {
            return 1;
        }
        const auto* ref_value = measinfo->find("Ref");
        const auto* ref_as_string =
            (ref_value == nullptr) ? nullptr : std::get_if<std::string>(&ref_value->storage());
        if (!expect_true(ref_as_string != nullptr && *ref_as_string == "ITRF",
                         "UVW MEASINFO.Ref mismatch")) {
            return 1;
        }

        const auto empty = casacore_mini::parse_showtableinfo_keywords("Structure of table t\n");
        if (!expect_true(empty.table_keywords.size() == 0U && empty.column_keywords.empty(),
                         "empty keyword parse result mismatch")) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "keyword_record_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
