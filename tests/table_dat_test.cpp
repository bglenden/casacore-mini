#include "casacore_mini/table_dat.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

bool expect_true(const bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path.string());
    }

    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)),
                                     std::istreambuf_iterator<char>());
}

bool test_v0_fixture() {
    const std::filesystem::path fixture_path =
        std::filesystem::path(CASACORE_MINI_SOURCE_DIR) /
        "data/corpus/fixtures/table_dat_ttable2_v0/table.dat";

    const auto metadata = casacore_mini::read_table_dat_metadata(fixture_path.string());
    if (!expect_true(metadata.table_version == 1U, "v0 table_version mismatch")) {
        return false;
    }
    if (!expect_true(metadata.row_count == 10U, "v0 row_count mismatch")) {
        return false;
    }
    if (!expect_true(metadata.big_endian, "v0 endian flag mismatch")) {
        return false;
    }
    return expect_true(metadata.table_type == "PlainTable", "v0 table_type mismatch");
}

bool test_v1_fixture() {
    const std::filesystem::path fixture_path =
        std::filesystem::path(CASACORE_MINI_SOURCE_DIR) /
        "data/corpus/fixtures/table_dat_ttable2_v1/table.dat";

    const auto metadata = casacore_mini::read_table_dat_metadata(fixture_path.string());
    if (!expect_true(metadata.table_version == 2U, "v1 table_version mismatch")) {
        return false;
    }
    if (!expect_true(metadata.row_count == 10U, "v1 row_count mismatch")) {
        return false;
    }
    if (!expect_true(metadata.big_endian, "v1 endian flag mismatch")) {
        return false;
    }
    return expect_true(metadata.table_type == "PlainTable", "v1 table_type mismatch");
}

bool test_rejects_invalid_endian_flag() {
    const std::filesystem::path fixture_path =
        std::filesystem::path(CASACORE_MINI_SOURCE_DIR) /
        "data/corpus/fixtures/table_dat_ttable2_v0/table.dat";
    auto bytes = read_bytes(fixture_path);
    if (bytes.size() < 28U) {
        throw std::runtime_error("fixture too short for endian mutation");
    }
    bytes[24] = 0U;
    bytes[25] = 0U;
    bytes[26] = 0U;
    bytes[27] = 2U;

    try {
        static_cast<void>(casacore_mini::parse_table_dat_metadata(std::span(bytes)));
    } catch (const std::exception&) {
        return true;
    }

    std::cerr << "parser accepted invalid table.dat endian flag\n";
    return false;
}

bool test_rejects_non_table_root_type() {
    const std::filesystem::path fixture_path =
        std::filesystem::path(CASACORE_MINI_SOURCE_DIR) /
        "data/corpus/fixtures/table_dat_ttable2_v0/table.dat";
    auto bytes = read_bytes(fixture_path);
    if (bytes.size() < 17U) {
        throw std::runtime_error("fixture too short for type mutation");
    }
    bytes[16] = static_cast<std::uint8_t>('X');

    try {
        static_cast<void>(casacore_mini::parse_table_dat_metadata(std::span(bytes)));
    } catch (const std::exception&) {
        return true;
    }

    std::cerr << "parser accepted non-Table table.dat root type\n";
    return false;
}

} // namespace

int main() noexcept {
    try {
        if (!test_v0_fixture()) {
            return 1;
        }
        if (!test_v1_fixture()) {
            return 1;
        }
        if (!test_rejects_invalid_endian_flag()) {
            return 1;
        }
        if (!test_rejects_non_table_root_type()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_dat_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
