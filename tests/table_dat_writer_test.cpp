// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_dat.hpp"
#include "casacore_mini/table_dat_writer.hpp"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool expect_true(const bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

/// Write a table.dat header, read it back, verify all fields match.
bool test_round_trip() {
    casacore_mini::TableDatMetadata original;
    original.table_version = 2;
    original.row_count = 375;
    original.big_endian = false;
    original.table_type = "PlainTable";

    auto bytes = casacore_mini::serialize_table_dat_header(original);
    const auto parsed = casacore_mini::parse_table_dat_metadata(bytes);

    // Writer always writes version 2.
    if (!expect_true(parsed.table_version == 2U, "table_version mismatch")) {
        return false;
    }
    if (!expect_true(parsed.row_count == 375U, "row_count mismatch")) {
        return false;
    }
    if (!expect_true(parsed.big_endian == false, "big_endian mismatch")) {
        return false;
    }
    if (!expect_true(parsed.table_type == "PlainTable", "table_type mismatch")) {
        return false;
    }
    return true;
}

/// Write header matching table_dat_ttable2_v1 fixture params, compare prefix bytes.
bool test_matches_fixture_prefix() {
    const auto root = std::filesystem::path(CASACORE_MINI_SOURCE_DIR);
    const auto fixture_path = root / "data/corpus/fixtures/table_dat_ttable2_v1/table.dat";

    std::ifstream input(fixture_path, std::ios::binary);
    if (!input) {
        std::cerr << "cannot open fixture: " << fixture_path << '\n';
        return false;
    }
    std::vector<std::uint8_t> fixture_bytes((std::istreambuf_iterator<char>(input)),
                                            std::istreambuf_iterator<char>());

    // Parse the fixture to learn its metadata.
    const auto fixture_meta = casacore_mini::parse_table_dat_metadata(fixture_bytes);

    // Write our own header with the same metadata.
    auto written = casacore_mini::serialize_table_dat_header(fixture_meta);

    // The written header should match the fixture bytes for the header portion.
    if (!expect_true(written.size() <= fixture_bytes.size(),
                     "written header larger than fixture")) {
        return false;
    }

    // Compare byte-by-byte, skipping the 4-byte object_length field at offsets 4-7.
    // The fixture's object_length covers the entire table.dat body (TableDesc, etc.),
    // while our writer only produces the header portion.
    for (std::size_t index = 0; index < written.size(); ++index) {
        if (index >= 4 && index < 8) {
            continue; // skip object_length field
        }
        if (written[index] != fixture_bytes[index]) {
            std::cerr << "byte mismatch at offset " << index
                      << ": written=" << static_cast<int>(written[index])
                      << " fixture=" << static_cast<int>(fixture_bytes[index]) << '\n';
            return false;
        }
    }
    return true;
}

bool test_rejects_row_count_overflow() {
    casacore_mini::TableDatMetadata metadata;
    metadata.table_version = 2U;
    metadata.row_count =
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL;
    metadata.big_endian = false;
    metadata.table_type = "PlainTable";

    try {
        static_cast<void>(casacore_mini::serialize_table_dat_header(metadata));
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << "table_dat_writer accepted out-of-range row_count\n";
    return false;
}

} // namespace

int main() noexcept {
    try {
        if (!test_round_trip()) {
            return 1;
        }
        if (!test_matches_fixture_prefix()) {
            return 1;
        }
        if (!test_rejects_row_count_overflow()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "table_dat_writer_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
