// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/table_binary_schema.hpp"

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/record_metadata.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace casacore_mini {
namespace {

[[nodiscard]] std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open binary file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] Record decode_aipsio_file(const std::filesystem::path& path) {
    const auto bytes = read_binary_file(path);
    AipsIoReader reader(bytes);
    auto record = read_aipsio_record(reader);
    if (!reader.empty()) {
        throw std::runtime_error("trailing bytes after Record decode in: " + path.string());
    }
    return record;
}

/// Extract column name from a filename matching `column_<NAME>_keywords.bin`.
///
/// Returns empty string if the filename does not match the expected pattern.
[[nodiscard]] std::string extract_column_name(const std::string& filename) {
    static constexpr std::string_view kPrefix = "column_";
    static constexpr std::string_view kSuffix = "_keywords.bin";

    if (filename.size() <= kPrefix.size() + kSuffix.size()) {
        return {};
    }
    if (filename.substr(0, kPrefix.size()) != kPrefix) {
        return {};
    }
    if (filename.substr(filename.size() - kSuffix.size()) != kSuffix) {
        return {};
    }
    return filename.substr(kPrefix.size(), filename.size() - kPrefix.size() - kSuffix.size());
}

} // namespace

MeasureCoordinateMetadata read_table_binary_metadata(const std::string_view fixture_dir) {
    const std::filesystem::path dir(fixture_dir);

    Record table_keywords;
    const auto table_kw_path = dir / "table_keywords.bin";
    if (std::filesystem::exists(table_kw_path)) {
        table_keywords = decode_aipsio_file(table_kw_path);
    }

    std::vector<std::pair<std::string, Record>> column_keywords;
    if (std::filesystem::is_directory(dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto filename = entry.path().filename().string();
            const auto column_name = extract_column_name(filename);
            if (!column_name.empty()) {
                column_keywords.emplace_back(column_name, decode_aipsio_file(entry.path()));
            }
        }
    }

    return extract_record_metadata(table_keywords, column_keywords);
}

} // namespace casacore_mini
