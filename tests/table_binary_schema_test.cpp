#include "casacore_mini/measure_coord_metadata.hpp"
#include "casacore_mini/table_binary_schema.hpp"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string read_text_file(const std::filesystem::path& path) {
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
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

const casacore_mini::MeasureColumnMetadata*
find_column(const std::vector<casacore_mini::MeasureColumnMetadata>& columns,
            const std::string& name) {
    const auto it = std::find_if(
        columns.begin(), columns.end(),
        [&](const casacore_mini::MeasureColumnMetadata& col) { return col.column_name == name; });
    if (it == columns.end()) {
        return nullptr;
    }
    return &(*it);
}

/// Test logtable: binary path vs text path produce same measure metadata.
bool test_logtable_parity() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto fixture_dir = root / "data/corpus/fixtures/logtable_stdstman_keywords";

    const auto text = read_text_file(fixture_dir / "showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);
    const auto bin_metadata = casacore_mini::read_table_binary_metadata(fixture_dir.string());

    const auto* text_time = find_column(text_metadata.measure_columns, "TIME");
    const auto* bin_time = find_column(bin_metadata.measure_columns, "TIME");

    if (!expect_true(text_time != nullptr, "logtable: text TIME metadata missing")) {
        return false;
    }
    if (!expect_true(bin_time != nullptr, "logtable: binary TIME metadata missing")) {
        return false;
    }
    if (!expect_true(*text_time == *bin_time, "logtable: text vs binary TIME metadata mismatch")) {
        return false;
    }
    return true;
}

/// Test ms_tree: binary path vs text path produce same UVW measure metadata.
bool test_ms_tree_parity() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto fixture_dir = root / "data/corpus/fixtures/ms_tree";

    const auto text = read_text_file(fixture_dir / "showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);
    const auto bin_metadata = casacore_mini::read_table_binary_metadata(fixture_dir.string());

    const auto* text_uvw = find_column(text_metadata.measure_columns, "UVW");
    const auto* bin_uvw = find_column(bin_metadata.measure_columns, "UVW");

    if (!expect_true(text_uvw != nullptr, "ms_tree: text UVW metadata missing")) {
        return false;
    }
    if (!expect_true(bin_uvw != nullptr, "ms_tree: binary UVW metadata missing")) {
        return false;
    }
    if (!expect_true(*text_uvw == *bin_uvw, "ms_tree: text vs binary UVW metadata mismatch")) {
        return false;
    }
    return true;
}

/// Test pagedimage: binary path vs text path produce same coordinate metadata.
bool test_pagedimage_parity() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto fixture_dir = root / "data/corpus/fixtures/pagedimage_coords";

    const auto text = read_text_file(fixture_dir / "showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);
    const auto bin_metadata = casacore_mini::read_table_binary_metadata(fixture_dir.string());

    if (!expect_true(bin_metadata.coordinates.has_coords, "pagedimage: binary coords missing")) {
        return false;
    }
    if (!expect_true(bin_metadata.coordinates == text_metadata.coordinates,
                     "pagedimage: text vs binary coordinate metadata mismatch")) {
        return false;
    }
    return true;
}

/// Test that an empty directory produces empty metadata without error.
bool test_empty_directory() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    // Use a directory that exists but has no .bin files.
    const auto fixture_dir = root / "data/corpus/fixtures/replay_ttableaccess";

    if (!std::filesystem::is_directory(fixture_dir)) {
        // Skip if fixture directory doesn't exist.
        return true;
    }

    const auto metadata = casacore_mini::read_table_binary_metadata(fixture_dir.string());
    if (!expect_true(metadata.measure_columns.empty(), "empty dir: expected no measure columns")) {
        return false;
    }
    if (!expect_true(!metadata.coordinates.has_coords, "empty dir: expected no coordinates")) {
        return false;
    }
    return true;
}

} // namespace

int main() noexcept {
    try {
        if (!test_logtable_parity()) {
            return 1;
        }
        if (!test_ms_tree_parity()) {
            return 1;
        }
        if (!test_pagedimage_parity()) {
            return 1;
        }
        if (!test_empty_directory()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_binary_schema_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
