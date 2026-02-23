#include "casacore_mini/measure_coord_metadata.hpp"

#include <algorithm>
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

const casacore_mini::MeasureColumnMetadata*
find_column(const std::vector<casacore_mini::MeasureColumnMetadata>& columns,
            const std::string& name) {
    const auto it = std::find_if(columns.begin(), columns.end(),
                                 [&](const casacore_mini::MeasureColumnMetadata& column) {
                                     return column.column_name == name;
                                 });
    if (it == columns.end()) {
        return nullptr;
    }
    return &(*it);
}

} // namespace

int main() noexcept {
    try {
        const auto source_root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);

        const auto logtable_text = read_file(
            source_root / "data/corpus/fixtures/logtable_stdstman_keywords/showtableinfo.txt");
        const auto logtable_metadata =
            casacore_mini::parse_showtableinfo_measure_coordinate_metadata(logtable_text);

        const auto* logtable_time = find_column(logtable_metadata.measure_columns, "TIME");
        if (!expect_true(logtable_time != nullptr, "logtable TIME measure metadata missing")) {
            return 1;
        }
        if (!expect_true(logtable_time->quantum_units == std::vector<std::string>{"s"},
                         "logtable TIME units mismatch")) {
            return 1;
        }
        if (!expect_true(logtable_time->measure_type == std::optional<std::string>{"EPOCH"},
                         "logtable TIME measure_type mismatch")) {
            return 1;
        }
        if (!expect_true(logtable_time->measure_reference == std::optional<std::string>{"UTC"},
                         "logtable TIME measure_reference mismatch")) {
            return 1;
        }

        const auto ms_text =
            read_file(source_root / "data/corpus/fixtures/ms_tree/showtableinfo.txt");
        const auto ms_metadata =
            casacore_mini::parse_showtableinfo_measure_coordinate_metadata(ms_text);

        const auto* ms_uvw = find_column(ms_metadata.measure_columns, "UVW");
        if (!expect_true(ms_uvw != nullptr, "ms UVW measure metadata missing")) {
            return 1;
        }
        if (!expect_true(ms_uvw->quantum_units == std::vector<std::string>{"m", "m", "m"},
                         "ms UVW units mismatch")) {
            return 1;
        }
        if (!expect_true(ms_uvw->measure_type == std::optional<std::string>{"uvw"},
                         "ms UVW measure_type mismatch")) {
            return 1;
        }
        if (!expect_true(ms_uvw->measure_reference == std::optional<std::string>{"ITRF"},
                         "ms UVW measure_reference mismatch")) {
            return 1;
        }

        const auto* ms_time = find_column(ms_metadata.measure_columns, "TIME");
        if (!expect_true(ms_time != nullptr, "ms TIME measure metadata missing")) {
            return 1;
        }
        if (!expect_true(ms_time->quantum_units == std::vector<std::string>{"s"},
                         "ms TIME units mismatch")) {
            return 1;
        }
        if (!expect_true(ms_time->measure_type == std::optional<std::string>{"epoch"},
                         "ms TIME measure_type mismatch")) {
            return 1;
        }
        if (!expect_true(ms_time->measure_reference == std::optional<std::string>{"UTC"},
                         "ms TIME measure_reference mismatch")) {
            return 1;
        }

        const auto pagedimage_text =
            read_file(source_root / "data/corpus/fixtures/pagedimage_coords/showtableinfo.txt");
        const auto pagedimage_metadata =
            casacore_mini::parse_showtableinfo_measure_coordinate_metadata(pagedimage_text);

        if (!expect_true(pagedimage_metadata.coordinates.has_coords,
                         "coords record not detected")) {
            return 1;
        }
        if (!expect_true(pagedimage_metadata.coordinates.obsdate_type ==
                             std::optional<std::string>{"epoch"},
                         "coords obsdate_type mismatch")) {
            return 1;
        }
        if (!expect_true(pagedimage_metadata.coordinates.obsdate_reference ==
                             std::optional<std::string>{"LAST"},
                         "coords obsdate_reference mismatch")) {
            return 1;
        }
        if (!expect_true(pagedimage_metadata.coordinates.direction_system ==
                             std::optional<std::string>{"J2000"},
                         "coords direction_system mismatch")) {
            return 1;
        }
        if (!expect_true(pagedimage_metadata.coordinates.direction_axes ==
                             std::vector<std::string>{"Right Ascension", "Declination"},
                         "coords direction_axes mismatch")) {
            return 1;
        }

        const auto empty_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(
            "Structure of table /tmp/t\n------------------\n");
        if (!expect_true(empty_metadata.measure_columns.empty(),
                         "unexpected measure columns for input without keywords section")) {
            return 1;
        }
        if (!expect_true(!empty_metadata.coordinates.has_coords,
                         "unexpected coords marker for input without keywords section")) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "measure_coord_metadata_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
