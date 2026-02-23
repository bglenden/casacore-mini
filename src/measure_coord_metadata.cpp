#include "casacore_mini/measure_coord_metadata.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace casacore_mini {
namespace {

[[nodiscard]] std::string trim_copy(std::string_view input) {
    constexpr std::string_view kWhitespace = " \t\r\n";
    const auto begin = input.find_first_not_of(kWhitespace);
    if (begin == std::string_view::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(kWhitespace);
    return std::string(input.substr(begin, end - begin + 1U));
}

[[nodiscard]] std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t offset = 0;
    while (offset <= text.size()) {
        const auto newline = text.find('\n', offset);
        const auto end = (newline == std::string_view::npos) ? text.size() : newline;
        std::string line(text.substr(offset, end - offset));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
        if (newline == std::string_view::npos) {
            break;
        }
        offset = newline + 1U;
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> split_bracket_items(std::string_view text) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find(',', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        const auto value = trim_copy(text.substr(start, end - start));
        if (!value.empty()) {
            items.push_back(value);
        }
        if (end == text.size()) {
            break;
        }
        start = end + 1U;
    }
    return items;
}

[[nodiscard]] std::optional<std::vector<std::string>>
parse_bracket_list_from_following_line(const std::vector<std::string>& lines,
                                       const std::size_t start_index) {
    static const std::regex bracket_line_regex(R"(^\s*\[([^\]]*)\]\s*$)");
    for (std::size_t index = start_index; index < lines.size(); ++index) {
        const auto stripped = trim_copy(lines[index]);
        if (stripped.empty()) {
            continue;
        }
        std::smatch match;
        if (!std::regex_match(stripped, match, bracket_line_regex)) {
            return std::nullopt;
        }
        const std::string values = match[1].str();
        return split_bracket_items(values);
    }
    return std::nullopt;
}

[[nodiscard]] bool path_equals(const std::vector<std::string>& path,
                               const std::initializer_list<std::string_view> expected) {
    if (path.size() != expected.size()) {
        return false;
    }
    auto expected_it = expected.begin();
    for (std::size_t index = 0; index < path.size(); ++index, ++expected_it) {
        if (path[index] != *expected_it) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::pair<CoordinateKeywordMetadata, std::size_t>
parse_coords_block(const std::vector<std::string>& lines, const std::size_t start_index) {
    static const std::regex open_record_regex(R"(^([A-Za-z0-9_]+):\s*\{$)");
    static const std::regex string_value_regex(R"re(^([A-Za-z0-9_]+):\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex axes_declaration_regex(
        R"(^axes:\s+String array with shape\s+\[[^\]]*\]\s*$)");

    CoordinateKeywordMetadata metadata;
    metadata.has_coords = true;

    std::vector<std::string> path{"coords"};
    std::size_t depth = 1;
    std::size_t index = start_index + 1U;
    for (; index < lines.size() && depth > 0U; ++index) {
        const auto stripped = trim_copy(lines[index]);
        if (stripped.empty()) {
            continue;
        }

        if (stripped == "}") {
            if (!path.empty()) {
                path.pop_back();
            }
            if (depth > 0U) {
                --depth;
            }
            continue;
        }

        std::smatch open_match;
        if (std::regex_match(stripped, open_match, open_record_regex)) {
            path.push_back(open_match[1].str());
            ++depth;
            continue;
        }

        std::smatch value_match;
        if (std::regex_match(stripped, value_match, string_value_regex)) {
            const auto key = value_match[1].str();
            const auto value = value_match[2].str();

            if (path_equals(path, {"coords", "obsdate"})) {
                if (key == "type") {
                    metadata.obsdate_type = value;
                } else if (key == "refer") {
                    metadata.obsdate_reference = value;
                }
            } else if (path_equals(path, {"coords", "direction0"}) && key == "system") {
                metadata.direction_system = value;
            }
            continue;
        }

        if (path_equals(path, {"coords", "direction0"}) &&
            std::regex_match(stripped, axes_declaration_regex)) {
            const auto axes = parse_bracket_list_from_following_line(lines, index + 1U);
            if (axes.has_value()) {
                metadata.direction_axes = *axes;
            }
        }
    }

    if (index == 0U) {
        return std::pair(metadata, start_index);
    }
    return std::pair(metadata, index - 1U);
}

[[nodiscard]] MeasureColumnMetadata& ensure_column(std::vector<MeasureColumnMetadata>& columns,
                                                   std::string_view column_name) {
    const auto it =
        std::find_if(columns.begin(), columns.end(), [&](const MeasureColumnMetadata& value) {
            return value.column_name == column_name;
        });
    if (it != columns.end()) {
        return *it;
    }
    columns.push_back(MeasureColumnMetadata{
        .column_name = std::string(column_name),
        .quantum_units = {},
        .measure_type = std::nullopt,
        .measure_reference = std::nullopt,
    });
    return columns.back();
}

} // namespace

MeasureCoordinateMetadata
parse_showtableinfo_measure_coordinate_metadata(const std::string_view showtableinfo_text) {
    static const std::regex column_header_regex(R"(^Column\s+([A-Za-z0-9_]+)\s*$)");
    static const std::regex unit_regex(R"re(^UNIT:\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex quantum_units_regex(
        R"(^QuantumUnits:\s+String array with shape\s+\[[^\]]*\]\s*$)");
    static const std::regex measure_type_regex(R"re(^MEASURE_TYPE:\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex measure_reference_regex(
        R"re(^MEASURE_REFERENCE:\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex measure_info_open_regex(R"(^MEASINFO:\s*\{\s*$)");
    static const std::regex measure_info_type_regex(R"re(^type:\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex measure_info_ref_regex(R"re(^Ref:\s+String\s+"([^"]*)"\s*$)re");
    static const std::regex coords_open_regex(R"(^coords:\s*\{\s*$)");

    MeasureCoordinateMetadata metadata;

    const auto marker_index = showtableinfo_text.find("Keywords of main table");
    if (marker_index == std::string_view::npos) {
        return metadata;
    }

    const auto keywords_text =
        showtableinfo_text.substr(marker_index + std::string_view("Keywords of main table").size());
    const auto lines = split_lines(keywords_text);

    std::string current_column;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto stripped = trim_copy(lines[index]);
        if (stripped.empty()) {
            continue;
        }

        std::smatch match;
        if (std::regex_match(stripped, match, coords_open_regex)) {
            const auto coords_result = parse_coords_block(lines, index);
            metadata.coordinates = coords_result.first;
            index = coords_result.second;
            continue;
        }

        if (std::regex_match(stripped, match, column_header_regex)) {
            current_column = match[1].str();
            continue;
        }

        if (current_column.empty()) {
            continue;
        }

        if (std::regex_match(stripped, match, unit_regex)) {
            auto& column = ensure_column(metadata.measure_columns, current_column);
            column.quantum_units = std::vector<std::string>{match[1].str()};
            continue;
        }

        if (std::regex_match(stripped, quantum_units_regex)) {
            auto& column = ensure_column(metadata.measure_columns, current_column);
            const auto units = parse_bracket_list_from_following_line(lines, index + 1U);
            if (units.has_value()) {
                column.quantum_units = *units;
            }
            continue;
        }

        if (std::regex_match(stripped, match, measure_type_regex)) {
            auto& column = ensure_column(metadata.measure_columns, current_column);
            column.measure_type = match[1].str();
            continue;
        }

        if (std::regex_match(stripped, match, measure_reference_regex)) {
            auto& column = ensure_column(metadata.measure_columns, current_column);
            column.measure_reference = match[1].str();
            continue;
        }

        if (std::regex_match(stripped, measure_info_open_regex)) {
            auto& column = ensure_column(metadata.measure_columns, current_column);
            std::size_t depth = 1;
            std::size_t inner_index = index + 1U;
            for (; inner_index < lines.size() && depth > 0U; ++inner_index) {
                const auto inner = trim_copy(lines[inner_index]);
                if (inner.empty()) {
                    continue;
                }
                if (inner == "}") {
                    --depth;
                    continue;
                }
                if (inner.find('{') != std::string::npos) {
                    ++depth;
                    continue;
                }

                std::smatch inner_match;
                if (std::regex_match(inner, inner_match, measure_info_type_regex)) {
                    column.measure_type = inner_match[1].str();
                } else if (std::regex_match(inner, inner_match, measure_info_ref_regex)) {
                    column.measure_reference = inner_match[1].str();
                }
            }
            if (inner_index > 0U) {
                index = inner_index - 1U;
            }
        }
    }

    return metadata;
}

} // namespace casacore_mini
