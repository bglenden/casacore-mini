#include "casacore_mini/table_schema.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace casacore_mini {
namespace {

[[nodiscard]] std::string trim_copy(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())) != 0) {
        input.remove_prefix(1);
    }
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())) != 0) {
        input.remove_suffix(1);
    }
    return std::string(input);
}

[[nodiscard]] std::string lower_ascii(std::string_view input) {
    std::string value;
    value.reserve(input.size());
    for (const char ch : input) {
        value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return value;
}

[[nodiscard]] std::optional<std::size_t> parse_size(std::string_view input) {
    const auto text = trim_copy(input);
    if (text.empty()) {
        return std::nullopt;
    }

    std::size_t value = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::vector<std::size_t>> parse_shape(std::string_view descriptor) {
    static const std::regex shape_regex(R"(shape=\[([^\]]*)\])");
    std::cmatch match;
    if (!std::regex_search(descriptor.begin(), descriptor.end(), match, shape_regex)) {
        return std::nullopt;
    }

    const std::string_view shape_text(match[1].first, static_cast<std::size_t>(match[1].length()));
    std::vector<std::size_t> shape;

    std::size_t start = 0;
    while (start <= shape_text.size()) {
        std::size_t end = shape_text.find(',', start);
        if (end == std::string_view::npos) {
            end = shape_text.size();
        }

        const auto item = parse_size(shape_text.substr(start, end - start));
        if (item.has_value()) {
            shape.push_back(*item);
        }

        if (end == shape_text.size()) {
            break;
        }
        start = end + 1;
    }

    return shape;
}

[[nodiscard]] std::optional<std::size_t> parse_ndim(std::string_view descriptor) {
    static const std::regex ndim_regex(R"(ndim=(\d+))");
    std::cmatch match;
    if (!std::regex_search(descriptor.begin(), descriptor.end(), match, ndim_regex)) {
        return std::nullopt;
    }

    return parse_size(
        std::string_view(match[1].first, static_cast<std::size_t>(match[1].length())));
}

[[nodiscard]] bool starts_with(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    return text.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] bool contains(const std::vector<StorageManagerInfo>& values,
                            const StorageManagerInfo& candidate) {
    return std::find(values.begin(), values.end(), candidate) != values.end();
}

} // namespace

TableSchema parse_showtableinfo_schema(const std::string_view showtableinfo_text) {
    static const std::regex structure_regex(R"(^Structure of table[ \t]+(.+)$)");
    static const std::regex kind_regex(R"(^------------------[ \t]*(.*)$)");
    static const std::regex row_col_regex(
        R"(^[ \t]*([0-9]+)[ \t]+rows,[ \t]+([0-9]+)[ \t]+columns.*$)");
    static const std::regex manager_regex(
        R"(^ +([A-Za-z0-9_]+StMan)[ \t]+file=([^ \t]+)[ \t]+name=([^ \t]*).*$)");
    static const std::regex column_regex(R"(^  ([A-Za-z0-9_]+)[ \t]+([A-Za-z0-9_]+)[ \t]+(.+)$)");

    TableSchema schema;
    schema.table_kind = "table";

    std::optional<StorageManagerInfo> current_manager;
    bool in_schema = false;

    std::size_t offset = 0;
    while (offset <= showtableinfo_text.size()) {
        const auto newline = showtableinfo_text.find('\n', offset);
        const auto end = (newline == std::string_view::npos) ? showtableinfo_text.size() : newline;
        std::string line(showtableinfo_text.substr(offset, end - offset));
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (starts_with(line, "Keywords of main table")) {
            break;
        }

        std::smatch match;
        if (std::regex_match(line, match, structure_regex)) {
            schema.table_path = trim_copy(match[1].str());
            in_schema = true;
        } else if (in_schema && std::regex_match(line, match, kind_regex) &&
                   !match[1].str().empty()) {
            schema.table_kind = trim_copy(match[1].str());
        } else if (std::regex_match(line, match, row_col_regex)) {
            const auto rows = parse_size(match[1].str());
            const auto cols = parse_size(match[2].str());
            schema.row_count = rows.value_or(0);
            schema.column_count = cols.value_or(0);
        } else if (in_schema && std::regex_match(line, match, manager_regex)) {
            StorageManagerInfo manager{
                .manager_class = match[1].str(),
                .file = match[2].str(),
                .name = match[3].str(),
            };
            current_manager = manager;
            if (!contains(schema.storage_managers, manager)) {
                schema.storage_managers.push_back(std::move(manager));
            }
        } else if (in_schema && std::regex_match(line, match, column_regex)) {
            const std::string descriptor = trim_copy(match[3].str());
            const std::string lower_descriptor = lower_ascii(descriptor);
            const bool is_manager_metadata =
                starts_with(descriptor, "file=") ||
                (descriptor.find('=') != std::string::npos &&
                 (lower_descriptor.find("hypercubes") != std::string::npos ||
                  lower_descriptor.find("bucketsize") != std::string::npos));

            if (!is_manager_metadata) {
                std::string value_kind = "unknown";
                if (lower_descriptor.find("scalar") != std::string::npos) {
                    value_kind = "scalar";
                } else if (lower_descriptor.find("array") != std::string::npos ||
                           lower_descriptor.find("shape=") != std::string::npos ||
                           lower_descriptor.find("ndim=") != std::string::npos) {
                    value_kind = "array";
                }

                schema.columns.push_back(ColumnSchema{
                    .name = match[1].str(),
                    .data_type = match[2].str(),
                    .descriptor = descriptor,
                    .value_kind = value_kind,
                    .shape = parse_shape(descriptor),
                    .ndim = parse_ndim(descriptor),
                    .storage_manager = current_manager,
                });
            }
        }

        if (newline == std::string_view::npos) {
            break;
        }
        offset = newline + 1;
    }

    return schema;
}

} // namespace casacore_mini
