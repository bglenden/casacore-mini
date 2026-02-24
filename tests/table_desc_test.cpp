#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_desc_writer.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool expect_true(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

template <typename T> bool expect_eq(const T& actual, const T& expected, const std::string& label) {
    if (actual == expected) {
        return true;
    }
    std::cerr << "FAIL: " << label << '\n';
    return false;
}

[[nodiscard]] std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

/// Test parsing of v0 fixture (tableVer=1).
bool test_parse_v0() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto bytes =
        read_binary_file(root / "data/corpus/fixtures/table_dat_ttable2_v0/table.dat");
    const auto full = casacore_mini::parse_table_dat_full(bytes);

    if (!expect_eq(full.table_version, 1U, "v0: table_version")) {
        return false;
    }
    if (!expect_eq(full.row_count, std::uint64_t{10}, "v0: row_count")) {
        return false;
    }
    if (!expect_true(full.big_endian, "v0: big_endian")) {
        return false;
    }
    if (!expect_eq(full.table_type, std::string("PlainTable"), "v0: table_type")) {
        return false;
    }

    // TableDesc.
    if (!expect_eq(full.table_desc.version, 2U, "v0: TD version")) {
        return false;
    }
    if (!expect_eq(full.table_desc.name, std::string("1"), "v0: TD name")) {
        return false;
    }
    if (!expect_eq(full.table_desc.columns.size(), std::size_t{9}, "v0: column count")) {
        return false;
    }

    // First column: ab (ScalarColumnDesc<Int>).
    const auto& col0 = full.table_desc.columns[0];
    if (!expect_eq(col0.name, std::string("ab"), "v0: col0 name")) {
        return false;
    }
    if (!expect_eq(col0.kind, casacore_mini::ColumnKind::scalar, "v0: col0 kind")) {
        return false;
    }
    if (!expect_eq(col0.data_type, casacore_mini::DataType::tp_int, "v0: col0 dtype")) {
        return false;
    }
    if (!expect_eq(col0.ndim, 0, "v0: col0 ndim")) {
        return false;
    }

    // arr1 column (ArrayColumnDesc<float>, ndim=3, shape=[2,3,4]).
    const auto& col3 = full.table_desc.columns[3];
    if (!expect_eq(col3.name, std::string("arr1"), "v0: col3 name")) {
        return false;
    }
    if (!expect_eq(col3.kind, casacore_mini::ColumnKind::array, "v0: col3 kind")) {
        return false;
    }
    if (!expect_eq(col3.data_type, casacore_mini::DataType::tp_float, "v0: col3 dtype")) {
        return false;
    }
    if (!expect_eq(col3.ndim, 3, "v0: col3 ndim")) {
        return false;
    }
    if (!expect_eq(col3.shape.size(), std::size_t{3}, "v0: col3 shape size")) {
        return false;
    }
    if (!expect_eq(col3.shape[0], std::int64_t{2}, "v0: col3 shape[0]")) {
        return false;
    }
    if (!expect_eq(col3.shape[1], std::int64_t{3}, "v0: col3 shape[1]")) {
        return false;
    }
    if (!expect_eq(col3.shape[2], std::int64_t{4}, "v0: col3 shape[2]")) {
        return false;
    }

    // af column (ScalarColumnDesc<String>, max_length=10).
    const auto& col8 = full.table_desc.columns[8];
    if (!expect_eq(col8.name, std::string("af"), "v0: col8 name")) {
        return false;
    }
    if (!expect_eq(col8.max_length, 10, "v0: col8 max_length")) {
        return false;
    }

    // Storage managers.
    if (!expect_eq(full.storage_managers.size(), std::size_t{3}, "v0: SM count")) {
        return false;
    }

    // Column setups.
    if (!expect_eq(full.column_setups.size(), std::size_t{9}, "v0: col setup count")) {
        return false;
    }
    if (!expect_eq(full.column_setups[0].column_name, std::string("ab"), "v0: setup[0] col_name")) {
        return false;
    }
    if (!expect_eq(full.column_setups[0].sequence_number, 1U, "v0: setup[0] seq")) {
        return false;
    }

    // v1-specific: table_keywords_v1 should be present.
    if (!expect_true(full.table_keywords_v1.has_value(), "v0: table_keywords_v1 present")) {
        return false;
    }

    return true;
}

/// Test parsing of v1 fixture (tableVer=2).
bool test_parse_v1() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto bytes =
        read_binary_file(root / "data/corpus/fixtures/table_dat_ttable2_v1/table.dat");
    const auto full = casacore_mini::parse_table_dat_full(bytes);

    if (!expect_eq(full.table_version, 2U, "v1: table_version")) {
        return false;
    }
    if (!expect_eq(full.row_count, std::uint64_t{10}, "v1: row_count")) {
        return false;
    }
    if (!expect_true(full.big_endian, "v1: big_endian")) {
        return false;
    }
    if (!expect_eq(full.table_desc.columns.size(), std::size_t{9}, "v1: column count")) {
        return false;
    }

    // v2 format: no table_keywords_v1.
    if (!expect_true(!full.table_keywords_v1.has_value(), "v1: table_keywords_v1 absent")) {
        return false;
    }

    // Verify same column structure as v0.
    const auto& col0 = full.table_desc.columns[0];
    if (!expect_eq(col0.name, std::string("ab"), "v1: col0 name")) {
        return false;
    }
    if (!expect_eq(full.column_setups.size(), std::size_t{9}, "v1: col setup count")) {
        return false;
    }

    return true;
}

/// Test round-trip: parse v1 fixture -> serialize -> parse again -> compare.
bool test_roundtrip_v1() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto original_bytes =
        read_binary_file(root / "data/corpus/fixtures/table_dat_ttable2_v1/table.dat");
    const auto original = casacore_mini::parse_table_dat_full(original_bytes);

    // Serialize.
    const auto serialized_bytes = casacore_mini::serialize_table_dat_full(original);

    // Re-parse.
    const auto reparsed = casacore_mini::parse_table_dat_full(serialized_bytes);

    // Compare key fields (not byte-identical since we always write v2 format).
    if (!expect_eq(reparsed.table_version, 2U, "roundtrip: table_version")) {
        return false;
    }
    if (!expect_eq(reparsed.row_count, original.row_count, "roundtrip: row_count")) {
        return false;
    }
    if (!expect_eq(reparsed.table_type, original.table_type, "roundtrip: table_type")) {
        return false;
    }
    if (!expect_eq(reparsed.table_desc.name, original.table_desc.name, "roundtrip: TD name")) {
        return false;
    }
    if (!expect_eq(reparsed.table_desc.comment, original.table_desc.comment,
                   "roundtrip: TD comment")) {
        return false;
    }
    if (!expect_eq(reparsed.table_desc.columns.size(), original.table_desc.columns.size(),
                   "roundtrip: ncol")) {
        return false;
    }
    for (std::size_t i = 0; i < original.table_desc.columns.size(); ++i) {
        const auto& orig_col = original.table_desc.columns[i];
        const auto& new_col = reparsed.table_desc.columns[i];
        if (!expect_eq(new_col.name, orig_col.name,
                       "roundtrip: col[" + std::to_string(i) + "] name")) {
            return false;
        }
        if (!expect_eq(new_col.data_type, orig_col.data_type,
                       "roundtrip: col[" + std::to_string(i) + "] dtype")) {
            return false;
        }
        if (!expect_eq(new_col.ndim, orig_col.ndim,
                       "roundtrip: col[" + std::to_string(i) + "] ndim")) {
            return false;
        }
        if (!expect_eq(new_col.shape, orig_col.shape,
                       "roundtrip: col[" + std::to_string(i) + "] shape")) {
            return false;
        }
    }

    if (!expect_eq(reparsed.storage_managers.size(), original.storage_managers.size(),
                   "roundtrip: SM count")) {
        return false;
    }
    if (!expect_eq(reparsed.column_setups.size(), original.column_setups.size(),
                   "roundtrip: col setup count")) {
        return false;
    }
    for (std::size_t i = 0; i < original.column_setups.size(); ++i) {
        if (!expect_eq(reparsed.column_setups[i].column_name, original.column_setups[i].column_name,
                       "roundtrip: setup[" + std::to_string(i) + "] col_name")) {
            return false;
        }
    }

    return true;
}

/// Test that truncated input is rejected.
bool test_malformed_truncated() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto bytes =
        read_binary_file(root / "data/corpus/fixtures/table_dat_ttable2_v1/table.dat");

    // Truncate at various points.
    for (std::size_t truncate_at : {16UL, 64UL, 256UL}) {
        if (truncate_at >= bytes.size()) {
            continue;
        }
        std::vector<std::uint8_t> truncated(
            bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(truncate_at));
        try {
            static_cast<void>(casacore_mini::parse_table_dat_full(truncated));
            std::cerr << "FAIL: truncated at " << truncate_at << " did not throw\n";
            return false;
        } catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch)
            // Expected: truncated input must throw.
        }
    }
    return true;
}

} // namespace

int main() noexcept {
    try {
        if (!test_parse_v0()) {
            return 1;
        }
        if (!test_parse_v1()) {
            return 1;
        }
        if (!test_roundtrip_v1()) {
            return 1;
        }
        if (!test_malformed_truncated()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_desc_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
