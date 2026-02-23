#include "casacore_mini/keyword_record.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

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

[[nodiscard]] std::string lower_ascii(std::string_view input) {
    std::string value;
    value.reserve(input.size());
    for (const char ch : input) {
        value.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return value;
}

[[nodiscard]] std::size_t leading_spaces(std::string_view line) {
    std::size_t count = 0;
    while (count < line.size() && line[count] == ' ') {
        ++count;
    }
    return count;
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

struct ParsedBracketLine {
    std::vector<std::string> items;
    std::size_t line_index = 0;
};

[[nodiscard]] std::optional<ParsedBracketLine>
parse_bracket_line_after(const std::vector<std::string>& lines, const std::size_t start_index) {
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

        return ParsedBracketLine{
            .items = split_bracket_items(match[1].str()),
            .line_index = index,
        };
    }
    return std::nullopt;
}

[[nodiscard]] bool parse_bool(std::string_view text, bool* value) {
    const auto normalized = lower_ascii(trim_copy(text));
    if (normalized == "1" || normalized == "true") {
        *value = true;
        return true;
    }
    if (normalized == "0" || normalized == "false") {
        *value = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool parse_int64(std::string_view text, std::int64_t* value) {
    const auto trimmed = trim_copy(text);
    if (trimmed.empty()) {
        return false;
    }
    const auto* begin = trimmed.data();
    const auto* end = begin + trimmed.size();
    const auto parse_result = std::from_chars(begin, end, *value);
    return parse_result.ec == std::errc() && parse_result.ptr == end;
}

[[nodiscard]] bool parse_double_value(std::string_view text, double* value) {
    const auto trimmed = trim_copy(text);
    if (trimmed.empty()) {
        return false;
    }
    char* parse_end = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &parse_end);
    if (parse_end == nullptr) {
        return false;
    }
    const auto parsed_chars = static_cast<std::size_t>(parse_end - trimmed.c_str());
    if (parsed_chars != trimmed.size()) {
        return false;
    }
    if (!std::isfinite(parsed)) {
        return false;
    }
    *value = parsed;
    return true;
}

[[nodiscard]] std::string decode_string_literal(std::string_view text) {
    auto trimmed = trim_copy(text);
    if (trimmed.size() >= 2U && trimmed.front() == '"' && trimmed.back() == '"') {
        return std::string(trimmed.substr(1U, trimmed.size() - 2U));
    }
    return trimmed;
}

[[nodiscard]] KeywordValue parse_scalar_value(const std::string& type_name,
                                              std::string_view value_text) {
    const auto type = lower_ascii(type_name);
    if (type == "string" || type == "table") {
        return KeywordValue(decode_string_literal(value_text));
    }

    if (type == "bool") {
        bool value = false;
        if (!parse_bool(value_text, &value)) {
            throw std::runtime_error("invalid Bool keyword value: " + trim_copy(value_text));
        }
        return KeywordValue(value);
    }

    if (type == "int") {
        std::int64_t value = 0;
        if (!parse_int64(value_text, &value)) {
            throw std::runtime_error("invalid Int keyword value: " + trim_copy(value_text));
        }
        return KeywordValue(value);
    }

    if (type == "float" || type == "double") {
        double value = 0.0;
        if (!parse_double_value(value_text, &value)) {
            throw std::runtime_error("invalid floating keyword value: " + trim_copy(value_text));
        }
        return KeywordValue(value);
    }

    throw std::runtime_error("unsupported keyword scalar type: " + type_name);
}

[[nodiscard]] KeywordValue parse_array_value(const std::string& element_type,
                                             const std::vector<std::string>& items) {
    KeywordArray array;
    array.elements.reserve(items.size());
    for (const auto& item : items) {
        array.elements.push_back(parse_scalar_value(element_type, item));
    }
    return KeywordValue::from_array(std::move(array));
}

[[nodiscard]] std::pair<std::string, std::string>
split_type_and_value(std::string_view value_text) {
    const auto trimmed = trim_copy(value_text);
    if (trimmed.empty()) {
        throw std::runtime_error("missing value after keyword type");
    }

    const auto first_space = trimmed.find(' ');
    if (first_space == std::string::npos) {
        throw std::runtime_error("keyword value does not contain type and payload: " + trimmed);
    }

    const auto type = trim_copy(trimmed.substr(0, first_space));
    const auto value = trim_copy(trimmed.substr(first_space + 1U));
    if (type.empty() || value.empty()) {
        throw std::runtime_error("keyword value has empty type or payload: " + trimmed);
    }
    return {type, value};
}

KeywordRecord parse_record(const std::vector<std::string>& lines, std::size_t* index,
                           const std::size_t min_indent) {
    static const std::regex entry_regex(R"(^([A-Za-z0-9_]+):\s*(.*)$)");

    KeywordRecord record;
    for (; *index < lines.size(); ++(*index)) {
        const std::string& line = lines[*index];
        const auto stripped = trim_copy(line);
        if (stripped.empty()) {
            continue;
        }

        const auto indent = leading_spaces(line);
        if (stripped == "}") {
            ++(*index);
            return record;
        }

        if (indent < min_indent) {
            return record;
        }

        std::smatch entry_match;
        if (!std::regex_match(stripped, entry_match, entry_regex)) {
            if (indent == min_indent && stripped.rfind("Column ", 0) == 0) {
                return record;
            }
            throw std::runtime_error("invalid keyword entry line: " + stripped);
        }

        const auto key = entry_match[1].str();
        const auto value_text = trim_copy(entry_match[2].str());

        if (value_text == "{") {
            ++(*index);
            auto nested = parse_record(lines, index, indent + 2U);
            record.set(key, KeywordValue::from_record(std::move(nested)));
            --(*index);
            continue;
        }

        const auto lowered_value_text = lower_ascii(value_text);
        const auto array_token_pos = lowered_value_text.find(" array with shape ");
        if (array_token_pos != std::string::npos) {
            const auto element_type = trim_copy(value_text.substr(0, array_token_pos));
            const auto array_items = parse_bracket_line_after(lines, *index + 1U);
            if (!array_items.has_value()) {
                throw std::runtime_error("missing bracket array payload for keyword: " + key);
            }
            record.set(key, parse_array_value(element_type, array_items->items));
            *index = array_items->line_index;
            continue;
        }

        const auto [type_name, scalar_text] = split_type_and_value(value_text);
        record.set(key, parse_scalar_value(type_name, scalar_text));
    }
    return record;
}

} // namespace

KeywordValue::KeywordValue() : storage_(false) {}

KeywordValue::KeywordValue(const bool value) : storage_(value) {}

KeywordValue::KeywordValue(const std::int64_t value) : storage_(value) {}

KeywordValue::KeywordValue(const double value) : storage_(value) {}

KeywordValue::KeywordValue(std::string value) : storage_(std::move(value)) {}

KeywordValue::KeywordValue(const char* value) : storage_(std::string(value)) {}

KeywordValue KeywordValue::from_array(KeywordArray value) {
    return KeywordValue(std::make_shared<KeywordArray>(std::move(value)));
}

KeywordValue KeywordValue::from_record(KeywordRecord value) {
    return KeywordValue(std::make_shared<KeywordRecord>(std::move(value)));
}

const KeywordValue::storage_type& KeywordValue::storage() const noexcept {
    return storage_;
}

KeywordValue::KeywordValue(array_ptr value) : storage_(std::move(value)) {
    if (!std::get<array_ptr>(storage_)) {
        throw std::invalid_argument("Keyword array pointer must not be null");
    }
}

KeywordValue::KeywordValue(record_ptr value) : storage_(std::move(value)) {
    if (!std::get<record_ptr>(storage_)) {
        throw std::invalid_argument("Keyword record pointer must not be null");
    }
}

bool KeywordValue::operator==(const KeywordValue& other) const {
    if (storage_.index() != other.storage_.index()) {
        return false;
    }

    return std::visit(
        [&](const auto& left) {
            using value_type = std::decay_t<decltype(left)>;
            const auto* right = std::get_if<value_type>(&other.storage_);
            if (right == nullptr) {
                return false;
            }
            if constexpr (std::is_same_v<value_type, array_ptr> ||
                          std::is_same_v<value_type, record_ptr>) {
                return *left == **right;
            } else {
                return left == *right;
            }
        },
        storage_);
}

bool KeywordArray::operator==(const KeywordArray& other) const {
    return elements == other.elements;
}

void KeywordRecord::set(const std::string_view key, KeywordValue value) {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const entry& item) { return item.first == key; });
    if (it != entries_.end()) {
        it->second = std::move(value);
        return;
    }
    entries_.emplace_back(std::string(key), std::move(value));
}

const KeywordValue* KeywordRecord::find(const std::string_view key) const {
    const auto it = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const entry& item) { return item.first == key; });
    if (it == entries_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t KeywordRecord::size() const noexcept {
    return entries_.size();
}

const std::vector<KeywordRecord::entry>& KeywordRecord::entries() const noexcept {
    return entries_;
}

bool KeywordRecord::operator==(const KeywordRecord& other) const {
    return entries_ == other.entries_;
}

const KeywordRecord* ShowtableinfoKeywords::find_column(const std::string_view column_name) const {
    const auto it = std::find_if(column_keywords.begin(), column_keywords.end(),
                                 [&](const std::pair<std::string, KeywordRecord>& entry) {
                                     return entry.first == column_name;
                                 });
    if (it == column_keywords.end()) {
        return nullptr;
    }
    return &it->second;
}

bool ShowtableinfoKeywords::operator==(const ShowtableinfoKeywords& other) const {
    return table_keywords == other.table_keywords && column_keywords == other.column_keywords;
}

ShowtableinfoKeywords parse_showtableinfo_keywords(const std::string_view showtableinfo_text) {
    ShowtableinfoKeywords parsed;
    const auto marker_index = showtableinfo_text.find("Keywords of main table");
    if (marker_index == std::string_view::npos) {
        return parsed;
    }

    const auto keyword_text =
        showtableinfo_text.substr(marker_index + std::string_view("Keywords of main table").size());
    const auto lines = split_lines(keyword_text);

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto stripped = trim_copy(lines[index]);
        if (stripped == "Table Keywords") {
            ++index;
            parsed.table_keywords = parse_record(lines, &index, 4U);
            if (index > 0U) {
                --index;
            }
            continue;
        }

        if (stripped.rfind("Column ", 0) == 0) {
            const auto column_name = trim_copy(stripped.substr(std::string_view("Column ").size()));
            ++index;
            auto record = parse_record(lines, &index, 4U);
            parsed.column_keywords.emplace_back(column_name, std::move(record));
            if (index > 0U) {
                --index;
            }
            continue;
        }
    }

    return parsed;
}

} // namespace casacore_mini
