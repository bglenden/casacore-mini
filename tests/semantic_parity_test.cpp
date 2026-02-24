#include "casacore_mini/record.hpp"
#include "casacore_mini/table_dat.hpp"
#include "casacore_mini/table_directory.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

// --- Minimal helpers duplicated from interop_mini_tool.cpp ---

[[nodiscard]] std::string base64_encode(const std::string_view input) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2U) / 3U) * 4U);

    std::size_t index = 0;
    while (index + 2U < input.size()) {
        const auto a = static_cast<std::uint8_t>(input[index]);
        const auto b = static_cast<std::uint8_t>(input[index + 1U]);
        const auto c = static_cast<std::uint8_t>(input[index + 2U]);
        output.push_back(kAlphabet[(a >> 2U) & 0x3FU]);
        output.push_back(kAlphabet[((a & 0x03U) << 4U) | ((b >> 4U) & 0x0FU)]);
        output.push_back(kAlphabet[((b & 0x0FU) << 2U) | ((c >> 6U) & 0x03U)]);
        output.push_back(kAlphabet[c & 0x3FU]);
        index += 3U;
    }
    const auto remaining = input.size() - index;
    if (remaining == 1U) {
        const auto a = static_cast<std::uint8_t>(input[index]);
        output.push_back(kAlphabet[(a >> 2U) & 0x3FU]);
        output.push_back(kAlphabet[(a & 0x03U) << 4U]);
        output.push_back('=');
        output.push_back('=');
    } else if (remaining == 2U) {
        const auto a = static_cast<std::uint8_t>(input[index]);
        const auto b = static_cast<std::uint8_t>(input[index + 1U]);
        output.push_back(kAlphabet[(a >> 2U) & 0x3FU]);
        output.push_back(kAlphabet[((a & 0x03U) << 4U) | ((b >> 4U) & 0x0FU)]);
        output.push_back(kAlphabet[(b & 0x0FU) << 2U]);
        output.push_back('=');
    }
    return output;
}

template <typename float_t> [[nodiscard]] std::string format_hex_float(const float_t value) {
    std::ostringstream output;
    output << std::hexfloat << value;
    return output.str();
}

[[nodiscard]] std::string field_path(const std::string_view prefix,
                                     const std::string_view field_name) {
    if (prefix.empty()) {
        return std::string(field_name);
    }
    return std::string(prefix) + "." + std::string(field_name);
}

[[nodiscard]] std::string join_shape(const std::vector<std::uint64_t>& shape) {
    std::ostringstream output;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0U) {
            output << ',';
        }
        output << shape[i];
    }
    return output.str();
}

void append_record_lines(const casacore_mini::Record& record, const std::string_view prefix,
                         std::vector<std::string>* lines);

template <typename element_t, typename formatter_t>
void append_array_line(const std::string_view path, const std::string_view element_type,
                       const casacore_mini::RecordArray<element_t>& array, formatter_t&& formatter,
                       std::vector<std::string>* lines) {
    std::ostringstream values_text;
    for (std::size_t i = 0; i < array.elements.size(); ++i) {
        if (i > 0U) {
            values_text << ',';
        }
        values_text << formatter(array.elements[i]);
    }
    lines->push_back("field=" + std::string(path) + "|array:" + std::string(element_type) +
                     "|shape=" + join_shape(array.shape) + "|values=" + values_text.str());
}

void append_value_line(const std::string_view path, const casacore_mini::RecordValue& value,
                       std::vector<std::string>* lines) {
    const auto& storage = value.storage();
    if (const auto* v = std::get_if<std::int32_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|int32|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<double>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|float64|" + format_hex_float(*v));
        return;
    }
    if (const auto* v = std::get_if<std::string>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|string|b64:" + base64_encode(*v));
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::string_array>(&storage)) {
        append_array_line(
            path, "string", *v,
            [](const std::string& item) { return "b64:" + base64_encode(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::record_ptr>(&storage)) {
        if (*v == nullptr) {
            throw std::runtime_error("record_ptr value is null");
        }
        append_record_lines(**v, path, lines);
        return;
    }
    throw std::runtime_error("unsupported RecordValue type in test");
}

void append_record_lines(const casacore_mini::Record& record, const std::string_view prefix,
                         std::vector<std::string>* lines) {
    for (const auto& [name, value] : record.entries()) {
        append_value_line(field_path(prefix, name), value, lines);
    }
}

[[nodiscard]] std::vector<std::string> canonical_record_lines(const casacore_mini::Record& record) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=record");
    append_record_lines(record, "", &lines);
    return lines;
}

void verify_lines_equal(const std::string_view label, const std::vector<std::string>& expected,
                        const std::vector<std::string>& actual) {
    if (expected == actual) {
        return;
    }
    const std::size_t max_shared = std::min(expected.size(), actual.size());
    std::size_t mismatch_index = max_shared;
    for (std::size_t i = 0; i < max_shared; ++i) {
        if (expected[i] != actual[i]) {
            mismatch_index = i;
            break;
        }
    }
    std::ostringstream message;
    message << label << " mismatch";
    if (mismatch_index == max_shared) {
        message << ": line-count differs expected=" << expected.size()
                << " actual=" << actual.size();
    } else {
        message << " at line " << (mismatch_index + 1U);
        message << "\nexpected: " << expected[mismatch_index];
        message << "\nactual:   " << actual[mismatch_index];
    }
    throw std::runtime_error(message.str());
}

// --- Builder for expected SSM keywords (matches interop_mini_tool) ---

[[nodiscard]] casacore_mini::Record build_ssm_table_keywords() {
    casacore_mini::Record kw;
    kw.set("observer", casacore_mini::RecordValue(std::string("test_user")));
    kw.set("telescope", casacore_mini::RecordValue(std::string("VLA")));
    kw.set("version", casacore_mini::RecordValue(std::int32_t{2}));
    return kw;
}

[[nodiscard]] casacore_mini::Record build_ssm_value_col_keywords() {
    casacore_mini::Record kw;
    kw.set("UNIT", casacore_mini::RecordValue(std::string("Jy")));
    return kw;
}

// --- Tests ---

bool test_wrong_table_keyword_detected() {
    auto expected = build_ssm_table_keywords();
    casacore_mini::Record wrong;
    wrong.set("observer", casacore_mini::RecordValue(std::string("WRONG")));
    wrong.set("telescope", casacore_mini::RecordValue(std::string("VLA")));
    wrong.set("version", casacore_mini::RecordValue(std::int32_t{2}));

    bool threw = false;
    try {
        verify_lines_equal("test", canonical_record_lines(expected), canonical_record_lines(wrong));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cerr << "  PASS: test_wrong_table_keyword_detected\n";
    return true;
}

bool test_missing_col_keyword_detected() {
    auto expected = build_ssm_value_col_keywords();
    casacore_mini::Record empty;

    bool threw = false;
    try {
        verify_lines_equal("test", canonical_record_lines(expected), canonical_record_lines(empty));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cerr << "  PASS: test_missing_col_keyword_detected\n";
    return true;
}

bool test_extra_keyword_detected() {
    auto expected = build_ssm_table_keywords();
    casacore_mini::Record extra;
    extra.set("observer", casacore_mini::RecordValue(std::string("test_user")));
    extra.set("telescope", casacore_mini::RecordValue(std::string("VLA")));
    extra.set("version", casacore_mini::RecordValue(std::int32_t{2}));
    extra.set("spurious", casacore_mini::RecordValue(std::string("extra")));

    bool threw = false;
    try {
        verify_lines_equal("test", canonical_record_lines(expected), canonical_record_lines(extra));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cerr << "  PASS: test_extra_keyword_detected\n";
    return true;
}

bool test_wrong_sm_type_detected() {
    namespace fs = std::filesystem;

    // Build an SSM table with the wrong SM type_name.
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 1;
    full.big_endian = false;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_wrong_sm";

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::scalar;
    col.name = "x";
    col.data_type = casacore_mini::DataType::tp_int;
    col.dm_type = "IncrementalStMan"; // Wrong for SSM table
    col.dm_group = "ISMData";
    col.type_string = "ScalarColumnDesc<Int     ";
    col.version = 1;
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "IncrementalStMan"; // Wrong!
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "x";
    cms.sequence_number = 0;
    full.column_setups.push_back(cms);
    full.post_td_row_count = 1;

    // Write to temp directory, read back, check SM mapping.
    const auto tmp = fs::path(CASACORE_MINI_SOURCE_DIR) / "build-ci-local" / "test_wrong_sm_dir";
    fs::remove_all(tmp);
    casacore_mini::write_table_directory(tmp.string(), full);

    const auto td = casacore_mini::read_table_directory(tmp.string());

    // The SM type should be IncrementalStMan, which is wrong for an SSM table.
    // Verify the type is indeed what we wrote.
    assert(!td.table_dat.storage_managers.empty());
    assert(td.table_dat.storage_managers[0].type_name == "IncrementalStMan");

    // A strict check for "StandardStMan" should detect this mismatch.
    const auto& sm_type = td.table_dat.storage_managers[0].type_name;
    assert(sm_type != "StandardStMan");

    fs::remove_all(tmp);
    std::cerr << "  PASS: test_wrong_sm_type_detected\n";
    return true;
}

} // namespace

int main() noexcept {
    try {
        std::cerr << "semantic_parity_test:\n";
        if (!test_wrong_table_keyword_detected()) {
            return EXIT_FAILURE;
        }
        if (!test_missing_col_keyword_detected()) {
            return EXIT_FAILURE;
        }
        if (!test_extra_keyword_detected()) {
            return EXIT_FAILURE;
        }
        if (!test_wrong_sm_type_detected()) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << "semantic_parity_test threw: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
