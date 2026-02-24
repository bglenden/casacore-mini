#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/measure_coord_metadata.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/record_metadata.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
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

[[nodiscard]] std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
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
    const auto it = std::find_if(
        columns.begin(), columns.end(),
        [&](const casacore_mini::MeasureColumnMetadata& col) { return col.column_name == name; });
    if (it == columns.end()) {
        return nullptr;
    }
    return &(*it);
}

/// Test logtable: column TIME keywords from binary match text-parsed metadata.
bool test_logtable_cross_validation() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);

    // Text path.
    const auto text =
        read_text_file(root / "data/corpus/fixtures/logtable_stdstman_keywords/showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);

    // Binary path.
    const auto bin_bytes = read_binary_file(
        root / "data/corpus/fixtures/logtable_stdstman_keywords/column_TIME_keywords.bin");
    casacore_mini::AipsIoReader reader(bin_bytes);
    const auto time_record = casacore_mini::read_aipsio_record(reader);

    // Extract binary metadata.
    casacore_mini::Record empty_table_kw;
    std::vector<std::pair<std::string, casacore_mini::Record>> column_kws;
    column_kws.emplace_back("TIME", time_record);

    const auto bin_metadata = casacore_mini::extract_record_metadata(empty_table_kw, column_kws);

    // Cross-validate column-level metadata.
    const auto* text_time = find_column(text_metadata.measure_columns, "TIME");
    const auto* bin_time = find_column(bin_metadata.measure_columns, "TIME");

    if (!expect_true(text_time != nullptr, "text TIME metadata missing")) {
        return false;
    }
    if (!expect_true(bin_time != nullptr, "binary TIME metadata missing")) {
        return false;
    }
    if (!expect_true(*text_time == *bin_time, "logtable TIME metadata mismatch")) {
        return false;
    }
    return true;
}

/// Test ms_tree: table + UVW column keywords.
bool test_ms_tree_cross_validation() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);

    const auto text = read_text_file(root / "data/corpus/fixtures/ms_tree/showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);

    // Binary table keywords (no coordinate data in ms_tree).
    const auto table_kw_bytes =
        read_binary_file(root / "data/corpus/fixtures/ms_tree/table_keywords.bin");
    casacore_mini::AipsIoReader table_reader(table_kw_bytes);
    const auto table_record = casacore_mini::read_aipsio_record(table_reader);

    // Binary UVW column keywords.
    const auto uvw_bytes =
        read_binary_file(root / "data/corpus/fixtures/ms_tree/column_UVW_keywords.bin");
    casacore_mini::AipsIoReader uvw_reader(uvw_bytes);
    const auto uvw_record = casacore_mini::read_aipsio_record(uvw_reader);

    std::vector<std::pair<std::string, casacore_mini::Record>> column_kws;
    column_kws.emplace_back("UVW", uvw_record);

    const auto bin_metadata = casacore_mini::extract_record_metadata(table_record, column_kws);

    const auto* text_uvw = find_column(text_metadata.measure_columns, "UVW");
    const auto* bin_uvw = find_column(bin_metadata.measure_columns, "UVW");

    if (!expect_true(text_uvw != nullptr, "text UVW metadata missing")) {
        return false;
    }
    if (!expect_true(bin_uvw != nullptr, "binary UVW metadata missing")) {
        return false;
    }
    if (!expect_true(*text_uvw == *bin_uvw, "ms_tree UVW metadata mismatch")) {
        return false;
    }
    return true;
}

/// Test pagedimage: coordinate metadata from binary matches text-parsed metadata.
bool test_pagedimage_cross_validation() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);

    const auto text =
        read_text_file(root / "data/corpus/fixtures/pagedimage_coords/showtableinfo.txt");
    const auto text_metadata = casacore_mini::parse_showtableinfo_measure_coordinate_metadata(text);

    const auto table_kw_bytes =
        read_binary_file(root / "data/corpus/fixtures/pagedimage_coords/table_keywords.bin");
    casacore_mini::AipsIoReader reader(table_kw_bytes);
    const auto table_record = casacore_mini::read_aipsio_record(reader);

    std::vector<std::pair<std::string, casacore_mini::Record>> empty_columns;
    const auto bin_metadata = casacore_mini::extract_record_metadata(table_record, empty_columns);

    if (!expect_true(bin_metadata.coordinates.has_coords, "binary coords missing")) {
        return false;
    }
    if (!expect_true(bin_metadata.coordinates == text_metadata.coordinates,
                     "pagedimage coordinate metadata mismatch")) {
        return false;
    }
    return true;
}

} // namespace

int main() noexcept {
    try {
        if (!test_logtable_cross_validation()) {
            return 1;
        }
        if (!test_ms_tree_cross_validation()) {
            return 1;
        }
        if (!test_pagedimage_cross_validation()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "record_metadata_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
