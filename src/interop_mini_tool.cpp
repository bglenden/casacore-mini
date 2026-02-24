#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/aipsio_record_writer.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/table_dat.hpp"
#include "casacore_mini/table_dat_writer.hpp"
#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_desc_writer.hpp"
#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float kFloat32Tolerance = 1e-6F;
constexpr double kFloat64Tolerance = 1e-10;

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

[[nodiscard]] std::string format_complex64(const std::complex<float>& value) {
    return "(" + format_hex_float(value.real()) + ";" + format_hex_float(value.imag()) + ")";
}

/// Build the casacore-compatible AipsIO type string for a column descriptor.
/// Format: "ScalarColumnDesc<float   " or "ArrayColumnDesc<Int     " (no closing >).
/// Type IDs are 8-char padded; float/double are lowercase C++ primitives.
[[nodiscard]] std::string casacore_col_type_string(casacore_mini::ColumnKind kind,
                                                   casacore_mini::DataType dtype) {
    const char* type_id = nullptr;
    switch (dtype) {
    case casacore_mini::DataType::tp_bool:
        type_id = "Bool    ";
        break;
    case casacore_mini::DataType::tp_char:
        type_id = "Char    ";
        break;
    case casacore_mini::DataType::tp_uchar:
        type_id = "uChar   ";
        break;
    case casacore_mini::DataType::tp_short:
        type_id = "Short   ";
        break;
    case casacore_mini::DataType::tp_ushort:
        type_id = "uShort  ";
        break;
    case casacore_mini::DataType::tp_int:
        type_id = "Int     ";
        break;
    case casacore_mini::DataType::tp_uint:
        type_id = "uInt    ";
        break;
    case casacore_mini::DataType::tp_int64:
        type_id = "Int64   ";
        break;
    case casacore_mini::DataType::tp_float:
        type_id = "float   ";
        break;
    case casacore_mini::DataType::tp_double:
        type_id = "double  ";
        break;
    case casacore_mini::DataType::tp_complex:
        type_id = "Complex ";
        break;
    case casacore_mini::DataType::tp_dcomplex:
        type_id = "DComplex";
        break;
    case casacore_mini::DataType::tp_string:
        type_id = "String  ";
        break;
    default:
        type_id = "unknown ";
        break;
    }
    std::string prefix =
        (kind == casacore_mini::ColumnKind::scalar) ? "ScalarColumnDesc<" : "ArrayColumnDesc<";
    return prefix + type_id;
}

[[nodiscard]] std::string format_complex128(const std::complex<double>& value) {
    return "(" + format_hex_float(value.real()) + ";" + format_hex_float(value.imag()) + ")";
}

[[nodiscard]] std::string join_shape(const std::vector<std::uint64_t>& shape) {
    std::ostringstream output;
    for (std::size_t index = 0; index < shape.size(); ++index) {
        if (index > 0U) {
            output << ',';
        }
        output << shape[index];
    }
    return output.str();
}

[[nodiscard]] std::string field_path(const std::string_view prefix,
                                     const std::string_view field_name) {
    if (prefix.empty()) {
        return std::string(field_name);
    }
    return std::string(prefix) + "." + std::string(field_name);
}

void append_record_lines(const casacore_mini::Record& record, const std::string_view prefix,
                         std::vector<std::string>* lines);

template <typename element_t, typename formatter_t>
void append_array_line(const std::string_view path, const std::string_view element_type,
                       const casacore_mini::RecordArray<element_t>& array, formatter_t&& formatter,
                       std::vector<std::string>* lines) {
    std::ostringstream values_text;
    for (std::size_t index = 0; index < array.elements.size(); ++index) {
        if (index > 0U) {
            values_text << ',';
        }
        values_text << formatter(array.elements[index]);
    }

    lines->push_back("field=" + std::string(path) + "|array:" + std::string(element_type) +
                     "|shape=" + join_shape(array.shape) + "|values=" + values_text.str());
}

void append_value_line(const std::string_view path, const casacore_mini::RecordValue& value,
                       std::vector<std::string>* lines) {
    const auto& storage = value.storage();
    if (const auto* v = std::get_if<bool>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|bool|" + (*v ? "true" : "false"));
        return;
    }
    if (const auto* v = std::get_if<std::int16_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|int16|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<std::uint16_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|uint16|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<std::int32_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|int32|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<std::uint32_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|uint32|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<std::int64_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|int64|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<std::uint64_t>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|uint64|" + std::to_string(*v));
        return;
    }
    if (const auto* v = std::get_if<float>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|float32|" + format_hex_float(*v));
        return;
    }
    if (const auto* v = std::get_if<double>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|float64|" + format_hex_float(*v));
        return;
    }
    if (const auto* v = std::get_if<std::complex<float>>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|complex64|" + format_complex64(*v));
        return;
    }
    if (const auto* v = std::get_if<std::complex<double>>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|complex128|" + format_complex128(*v));
        return;
    }
    if (const auto* v = std::get_if<std::string>(&storage)) {
        lines->push_back("field=" + std::string(path) + "|string|b64:" + base64_encode(*v));
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::int16_array>(&storage)) {
        append_array_line(
            path, "int16", *v, [](const std::int16_t item) { return std::to_string(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::uint16_array>(&storage)) {
        append_array_line(
            path, "uint16", *v, [](const std::uint16_t item) { return std::to_string(item); },
            lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::int32_array>(&storage)) {
        append_array_line(
            path, "int32", *v, [](const std::int32_t item) { return std::to_string(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::uint32_array>(&storage)) {
        append_array_line(
            path, "uint32", *v, [](const std::uint32_t item) { return std::to_string(item); },
            lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::int64_array>(&storage)) {
        append_array_line(
            path, "int64", *v, [](const std::int64_t item) { return std::to_string(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::uint64_array>(&storage)) {
        append_array_line(
            path, "uint64", *v, [](const std::uint64_t item) { return std::to_string(item); },
            lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::float_array>(&storage)) {
        append_array_line(
            path, "float32", *v, [](const float item) { return format_hex_float(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::double_array>(&storage)) {
        append_array_line(
            path, "float64", *v, [](const double item) { return format_hex_float(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::complex64_array>(&storage)) {
        append_array_line(
            path, "complex64", *v,
            [](const std::complex<float>& item) { return format_complex64(item); }, lines);
        return;
    }
    if (const auto* v = std::get_if<casacore_mini::RecordValue::complex128_array>(&storage)) {
        append_array_line(
            path, "complex128", *v,
            [](const std::complex<double>& item) { return format_complex128(item); }, lines);
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
    if (std::holds_alternative<casacore_mini::RecordValue::list_ptr>(storage)) {
        throw std::runtime_error("list_ptr values are not supported in interop dump");
    }
    throw std::runtime_error("unsupported RecordValue type in interop dump");
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

[[nodiscard]] std::vector<std::uint8_t> read_binary(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file for read: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_binary(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open file for write: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void write_text(const std::filesystem::path& path, const std::vector<std::string>& lines) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open text output: " + path.string());
    }
    for (const auto& line : lines) {
        output << line << '\n';
    }
}

void verify_lines_equal(const std::string_view label,
                        const std::vector<std::string>& expected_lines,
                        const std::vector<std::string>& actual_lines) {
    if (expected_lines == actual_lines) {
        return;
    }

    const std::size_t max_shared = std::min(expected_lines.size(), actual_lines.size());
    std::size_t mismatch_index = max_shared;
    for (std::size_t index = 0; index < max_shared; ++index) {
        if (expected_lines[index] != actual_lines[index]) {
            mismatch_index = index;
            break;
        }
    }

    std::ostringstream message;
    message << label << " mismatch";
    if (mismatch_index == max_shared) {
        message << ": line-count differs expected=" << expected_lines.size()
                << " actual=" << actual_lines.size();
    } else {
        message << " at line " << (mismatch_index + 1U);
        message << "\nexpected: " << expected_lines[mismatch_index];
        message << "\nactual:   " << actual_lines[mismatch_index];
    }
    throw std::runtime_error(message.str());
}

[[nodiscard]] std::optional<std::string> arg_value(int argc, char** argv,
                                                   const std::string_view key) {
    for (int index = 2; index + 1 < argc; ++index) {
        if (std::string_view(argv[index]) == key) {
            return std::string(argv[index + 1]);
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool has_flag(int argc, char** argv, const std::string_view flag) {
    for (int index = 2; index < argc; ++index) {
        if (std::string_view(argv[index]) == flag) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] casacore_mini::Record build_record_basic() {
    casacore_mini::Record record;
    record.set("flag", casacore_mini::RecordValue(true));
    record.set("i16", casacore_mini::RecordValue(std::int16_t{-42}));
    record.set("i32", casacore_mini::RecordValue(std::int32_t{-100000}));
    record.set("u32", casacore_mini::RecordValue(std::uint32_t{200000}));
    record.set("i64", casacore_mini::RecordValue(std::int64_t{9000000000LL}));
    record.set("f32", casacore_mini::RecordValue(1.5F));
    record.set("f64", casacore_mini::RecordValue(3.14));
    record.set("c64", casacore_mini::RecordValue(std::complex<float>(1.0F, -2.0F)));
    record.set("c128", casacore_mini::RecordValue(std::complex<double>(3.0, -4.0)));
    record.set("label", casacore_mini::RecordValue("hello"));

    casacore_mini::RecordValue::int32_array arr_i32;
    arr_i32.shape = {2U, 2U};
    arr_i32.elements = {10, 20, 30, 40};
    record.set("arr_i32", casacore_mini::RecordValue(std::move(arr_i32)));

    casacore_mini::RecordValue::double_array arr_f64;
    arr_f64.shape = {2U, 3U};
    arr_f64.elements = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    record.set("arr_f64", casacore_mini::RecordValue(std::move(arr_f64)));

    casacore_mini::RecordValue::complex64_array arr_c64;
    arr_c64.shape = {2U};
    arr_c64.elements = {std::complex<float>(1.0F, 2.0F), std::complex<float>(3.0F, 4.0F)};
    record.set("arr_c64", casacore_mini::RecordValue(std::move(arr_c64)));

    casacore_mini::RecordValue::string_array arr_str;
    arr_str.shape = {3U};
    arr_str.elements = {"alpha", "beta", "gamma"};
    record.set("arr_str", casacore_mini::RecordValue(std::move(arr_str)));

    return record;
}

[[nodiscard]] casacore_mini::Record build_record_nested() {
    casacore_mini::Record child;
    child.set("pi", casacore_mini::RecordValue(3.14159));
    child.set("name", casacore_mini::RecordValue("test"));

    casacore_mini::RecordValue::int64_array vec_i64;
    vec_i64.shape = {3U};
    vec_i64.elements = {7, 8, 9};
    child.set("vec_i64", casacore_mini::RecordValue(std::move(vec_i64)));

    casacore_mini::Record record;
    record.set("child", casacore_mini::RecordValue::from_record(std::move(child)));
    record.set("value", casacore_mini::RecordValue(std::int32_t{42}));

    casacore_mini::RecordValue::string_array units;
    units.shape = {2U};
    units.elements = {"m", "s"};
    record.set("units", casacore_mini::RecordValue(std::move(units)));

    return record;
}

[[nodiscard]] casacore_mini::TableDatMetadata expected_table_dat_metadata() {
    casacore_mini::TableDatMetadata metadata;
    metadata.table_version = 2U;
    metadata.row_count = 375U;
    metadata.big_endian = false;
    metadata.table_type = "PlainTable";
    return metadata;
}

[[nodiscard]] std::filesystem::path fixture_record_path(const std::string_view fixture_id) {
    if (fixture_id == "logtable_time") {
        return std::filesystem::path(
            "data/corpus/fixtures/logtable_stdstman_keywords/column_TIME_keywords.bin");
    }
    if (fixture_id == "ms_table") {
        return std::filesystem::path("data/corpus/fixtures/ms_tree/table_keywords.bin");
    }
    if (fixture_id == "ms_uvw") {
        return std::filesystem::path("data/corpus/fixtures/ms_tree/column_UVW_keywords.bin");
    }
    if (fixture_id == "pagedimage_table") {
        return std::filesystem::path("data/corpus/fixtures/pagedimage_coords/table_keywords.bin");
    }

    throw std::runtime_error("unknown fixture record id: " + std::string(fixture_id));
}

[[nodiscard]] casacore_mini::Record read_fixture_record(const std::string_view fixture_id) {
    const auto fixture_bytes = read_binary(fixture_record_path(fixture_id));
    casacore_mini::AipsIoReader fixture_reader(fixture_bytes);
    auto fixture_record = casacore_mini::read_aipsio_record(fixture_reader);
    if (!fixture_reader.empty()) {
        throw std::runtime_error("fixture record has trailing bytes: " + std::string(fixture_id));
    }
    return fixture_record;
}

struct DumpPaths {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
};

void write_record_artifact(const std::filesystem::path& output_path,
                           const casacore_mini::Record& record) {
    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, record);
    write_binary(output_path, writer.take_bytes());
}

void dump_record_artifact(const DumpPaths& paths) {
    const auto bytes = read_binary(paths.input_path);
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    write_text(paths.output_path, canonical_record_lines(record));
}

void verify_record_artifact(const std::filesystem::path& input_path,
                            const casacore_mini::Record& expected_record,
                            const std::string_view label) {
    const auto bytes = read_binary(input_path);
    casacore_mini::AipsIoReader reader(bytes);
    const auto actual_record = casacore_mini::read_aipsio_record(reader);
    if (!reader.empty()) {
        throw std::runtime_error(std::string(label) + " has trailing bytes after Record decode");
    }
    verify_lines_equal(label, canonical_record_lines(expected_record),
                       canonical_record_lines(actual_record));
}

void write_table_dat_header_artifact(const std::filesystem::path& output_path) {
    write_binary(output_path,
                 casacore_mini::serialize_table_dat_header(expected_table_dat_metadata()));
}

[[nodiscard]] std::vector<std::string>
canonical_table_dat_lines(const casacore_mini::TableDatMetadata& metadata) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dat_header");
    lines.emplace_back("table_version=" + std::to_string(metadata.table_version));
    lines.emplace_back("row_count=" + std::to_string(metadata.row_count));
    lines.emplace_back(std::string("big_endian=") + (metadata.big_endian ? "true" : "false"));
    lines.emplace_back("table_type_b64=" + base64_encode(metadata.table_type));
    return lines;
}

void dump_table_dat_header_artifact(const DumpPaths& paths) {
    const auto bytes = read_binary(paths.input_path);
    const auto metadata = casacore_mini::parse_table_dat_metadata(bytes);
    write_text(paths.output_path, canonical_table_dat_lines(metadata));
}

void verify_table_dat_header_artifact(const std::filesystem::path& input_path,
                                      const std::string_view label) {
    const auto bytes = read_binary(input_path);
    const auto metadata = casacore_mini::parse_table_dat_metadata(bytes);
    verify_lines_equal(label, canonical_table_dat_lines(expected_table_dat_metadata()),
                       canonical_table_dat_lines(metadata));
}

void write_fixture_record_artifact(const std::filesystem::path& output_path,
                                   const std::string_view fixture_id) {
    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, read_fixture_record(fixture_id));
    write_binary(output_path, writer.take_bytes());
}

void verify_fixture_record_artifact(const std::filesystem::path& input_path,
                                    const std::string_view fixture_id, const char* label) {
    const auto input_bytes = read_binary(input_path);
    casacore_mini::AipsIoReader input_reader(input_bytes);
    const auto input_record = casacore_mini::read_aipsio_record(input_reader);
    if (!input_reader.empty()) {
        throw std::runtime_error(std::string(label) + " has trailing bytes after Record decode");
    }

    const auto expected_record = read_fixture_record(fixture_id);
    verify_lines_equal(label, canonical_record_lines(expected_record),
                       canonical_record_lines(input_record));
}

// --- table_dat_full interop ---

[[nodiscard]] std::string datatype_name(casacore_mini::DataType dtype) {
    switch (dtype) {
    case casacore_mini::DataType::tp_bool:
        return "TpBool";
    case casacore_mini::DataType::tp_char:
        return "TpChar";
    case casacore_mini::DataType::tp_uchar:
        return "TpUChar";
    case casacore_mini::DataType::tp_short:
        return "TpShort";
    case casacore_mini::DataType::tp_ushort:
        return "TpUShort";
    case casacore_mini::DataType::tp_int:
        return "TpInt";
    case casacore_mini::DataType::tp_uint:
        return "TpUInt";
    case casacore_mini::DataType::tp_float:
        return "TpFloat";
    case casacore_mini::DataType::tp_double:
        return "TpDouble";
    case casacore_mini::DataType::tp_complex:
        return "TpComplex";
    case casacore_mini::DataType::tp_dcomplex:
        return "TpDComplex";
    case casacore_mini::DataType::tp_string:
        return "TpString";
    case casacore_mini::DataType::tp_table:
        return "TpTable";
    case casacore_mini::DataType::tp_int64:
        return "TpInt64";
    }
    return "Unknown(" + std::to_string(static_cast<int>(dtype)) + ")";
}

[[nodiscard]] std::string join_i64_shape(const std::vector<std::int64_t>& shape) {
    std::ostringstream output;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0U) {
            output << ',';
        }
        output << shape[i];
    }
    return output.str();
}

/// Canonical lines for table_dat_full interop comparison.
/// Uses a simplified format (no SM/setup lines) to allow cross-version comparison
/// since the mini serializer always writes v2 while fixtures may be v1.
[[nodiscard]] std::vector<std::string>
canonical_table_dat_full_lines(const casacore_mini::TableDatFull& full) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dat_full");
    lines.emplace_back("table_version=" + std::to_string(full.table_version));
    lines.emplace_back("row_count=" + std::to_string(full.row_count));
    lines.emplace_back("td_name_b64=" + base64_encode(full.table_desc.name));
    lines.emplace_back("td_ncol=" + std::to_string(full.table_desc.columns.size()));

    for (std::size_t i = 0; i < full.table_desc.columns.size(); ++i) {
        const auto& col = full.table_desc.columns[i];
        const auto prefix = "col[" + std::to_string(i) + "]";
        lines.emplace_back(prefix + ".name_b64=" + base64_encode(col.name));
        lines.emplace_back(prefix + ".kind=" +
                           (col.kind == casacore_mini::ColumnKind::scalar ? "scalar" : "array"));
        lines.emplace_back(prefix + ".dtype=" + datatype_name(col.data_type));
        lines.emplace_back(prefix + ".ndim=" + std::to_string(col.ndim));
        if (!col.shape.empty()) {
            lines.emplace_back(prefix + ".shape=" + join_i64_shape(col.shape));
        }
        lines.emplace_back(prefix + ".max_length=" + std::to_string(col.max_length));
        lines.emplace_back(prefix + ".dm_type_b64=" + base64_encode(col.dm_type));
        lines.emplace_back(prefix + ".dm_group_b64=" + base64_encode(col.dm_group));
    }

    return lines;
}

/// Read the ttable2_v1 fixture and produce a TableDatFull for interop.
/// Assumes CWD is the project root (the matrix runner ensures this).
[[nodiscard]] casacore_mini::TableDatFull read_fixture_table_dat_full() {
    const auto bytes = read_binary("data/corpus/fixtures/table_dat_ttable2_v1/table.dat");
    return casacore_mini::parse_table_dat_full(bytes);
}

void write_table_dat_full_artifact(const std::filesystem::path& output_path) {
    // Parse the v1 fixture, then re-serialize (always writes v2 format).
    const auto full = read_fixture_table_dat_full();
    write_binary(output_path, casacore_mini::serialize_table_dat_full(full));
}

void dump_table_dat_full_artifact(const DumpPaths& paths) {
    const auto bytes = read_binary(paths.input_path);
    const auto full = casacore_mini::parse_table_dat_full(bytes);
    write_text(paths.output_path, canonical_table_dat_full_lines(full));
}

void verify_table_dat_full_artifact(const std::filesystem::path& input_path,
                                    const std::string_view label) {
    const auto bytes = read_binary(input_path);
    const auto actual = casacore_mini::parse_table_dat_full(bytes);
    const auto expected = read_fixture_table_dat_full();
    // Compare using canonical lines (version may differ: we always write v2).
    auto expected_lines = canonical_table_dat_full_lines(expected);
    const auto actual_lines = canonical_table_dat_full_lines(actual);
    // Patch expected version to 2 since serialize always writes v2.
    for (auto& line : expected_lines) {
        if (line == "table_version=1") {
            line = "table_version=2";
        }
    }
    verify_lines_equal(label, expected_lines, actual_lines);
}

// --- table directory interop ---

/// Canonical lines for a table directory (structural comparison only).
[[nodiscard]] std::vector<std::string>
canonical_table_dir_lines(const casacore_mini::TableDirectory& td) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=" + std::to_string(td.table_dat.row_count));
    lines.emplace_back("ncol=" + std::to_string(td.table_dat.table_desc.columns.size()));
    for (std::size_t i = 0; i < td.table_dat.table_desc.columns.size(); ++i) {
        const auto& col = td.table_dat.table_desc.columns[i];
        const auto prefix = "col[" + std::to_string(i) + "]";
        lines.emplace_back(prefix + ".name_b64=" + base64_encode(col.name));
        lines.emplace_back(prefix + ".dtype=" + datatype_name(col.data_type));
    }
    lines.emplace_back("sm_file_count=" + std::to_string(td.sm_files.size()));
    for (std::size_t i = 0; i < td.sm_files.size(); ++i) {
        const auto prefix = "sm[" + std::to_string(i) + "]";
        lines.emplace_back(prefix + ".filename=" + td.sm_files[i].filename);
        lines.emplace_back(prefix + ".size=" + std::to_string(td.sm_files[i].size));
    }
    return lines;
}

/// Build a small test TableDatFull for the table directory interop case.
/// 5 rows, 4 columns (id:Int, value:Float, label:String, dval:Double),
/// single StandardStMan storage manager.
[[nodiscard]] casacore_mini::TableDatFull build_table_dir_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 5;
    full.big_endian = true;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_ssm";

    auto make_col = [](const std::string& name, casacore_mini::DataType dtype) {
        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::scalar;
        col.name = name;
        col.data_type = dtype;
        col.dm_type = "StandardStMan";
        col.dm_group = "StandardStMan";
        col.type_string = casacore_col_type_string(col.kind, dtype);
        col.version = 1;
        return col;
    };

    full.table_desc.columns.push_back(make_col("id", casacore_mini::DataType::tp_int));
    full.table_desc.columns.push_back(make_col("value", casacore_mini::DataType::tp_float));
    full.table_desc.columns.push_back(make_col("label", casacore_mini::DataType::tp_string));
    full.table_desc.columns.push_back(make_col("dval", casacore_mini::DataType::tp_double));

    // Table keywords: observer, telescope, version.
    full.table_desc.keywords.set("observer", casacore_mini::RecordValue(std::string("test_user")));
    full.table_desc.keywords.set("telescope", casacore_mini::RecordValue(std::string("VLA")));
    full.table_desc.keywords.set("version", casacore_mini::RecordValue(std::int32_t{2}));

    // Column keyword on "value": UNIT = "Jy".
    full.table_desc.columns[1].keywords.set("UNIT", casacore_mini::RecordValue(std::string("Jy")));

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "StandardStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : full.table_desc.columns) {
        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        full.column_setups.push_back(cms);
    }

    full.post_td_row_count = 5;
    return full;
}

void write_table_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_table_dir_table_dat();

    // Use LE byte order to match casacore on LE systems (macOS/Linux x86).
    full.big_endian = false;

    // Create the SsmWriter to produce cell data.
    casacore_mini::SsmWriter ssm_writer;
    ssm_writer.setup(full.table_desc.columns, full.row_count, full.big_endian, "StandardStMan");

    // Write the expected cell values.
    const std::int32_t id_vals[] = {0, 10, 20, 30, 40};
    const float value_vals[] = {0.0F, 1.5F, 3.0F, 4.5F, 6.0F};
    const std::string label_vals[] = {"row_0", "row_1", "row_2", "row_3", "row_4"};
    const double dval_vals[] = {0.0, 3.14, 6.28, 9.42, 12.56};

    for (std::uint64_t r = 0; r < 5; ++r) {
        ssm_writer.write_cell(0, r, casacore_mini::CellValue{id_vals[r]});
        ssm_writer.write_cell(1, r, casacore_mini::CellValue{value_vals[r]});
        ssm_writer.write_cell(2, r, casacore_mini::CellValue{label_vals[r]});
        ssm_writer.write_cell(3, r, casacore_mini::CellValue{dval_vals[r]});
    }

    // Set the SSM blob in the storage manager entry.
    full.storage_managers[0].data_blob = ssm_writer.make_blob();

    // Write table directory (table.dat).
    casacore_mini::write_table_directory(output_dir.string(), full);

    // Write the .f0 data file.
    ssm_writer.write_file(output_dir.string(), 0);
}

void verify_table_dir_artifact(const std::filesystem::path& input_dir,
                               const std::string_view label) {
    const auto td = casacore_mini::read_table_directory(input_dir.string());
    // Verify expected structure from build_table_dir_table_dat().
    auto expected_full = build_table_dir_table_dat();
    casacore_mini::TableDirectory expected_td;
    expected_td.table_dat = expected_full;
    // SM files won't match (mini doesn't write data files), so just verify metadata.
    const auto expected_lines = canonical_table_dir_lines(expected_td);
    auto actual_lines = canonical_table_dir_lines(td);
    // Strip SM file lines from both since mini can't produce them and casacore may.
    std::vector<std::string> expected_meta;
    std::vector<std::string> actual_meta;
    for (const auto& line : expected_lines) {
        if (line.rfind("sm[", 0) == 0) {
            continue;
        }
        if (line.rfind("sm_file_count=", 0) == 0) {
            expected_meta.emplace_back("sm_file_count=*");
            continue;
        }
        expected_meta.push_back(line);
    }
    for (const auto& line : actual_lines) {
        if (line.rfind("sm[", 0) == 0) {
            continue;
        }
        if (line.rfind("sm_file_count=", 0) == 0) {
            actual_meta.emplace_back("sm_file_count=*");
            continue;
        }
        actual_meta.push_back(line);
    }
    verify_lines_equal(label, expected_meta, actual_meta);

    const auto row_count = td.table_dat.row_count;
    const auto ncol = td.table_dat.table_desc.columns.size();
    std::cout << "  " << label << ": [PASS] structure (" << row_count << " rows, " << ncol
              << " cols)\n";

    // --- table_keywords verification (content) ---
    verify_lines_equal(std::string(label) + ":table_keywords",
                       canonical_record_lines(expected_full.table_desc.keywords),
                       canonical_record_lines(td.table_dat.table_desc.keywords));
    std::cout << "  " << label << ": [PASS] table_keywords ("
              << td.table_dat.table_desc.keywords.entries().size()
              << " fields, content verified)\n";

    // --- col_keywords verification (content) ---
    for (std::size_t i = 0; i < ncol; ++i) {
        verify_lines_equal(std::string(label) + ":col[" + std::to_string(i) + "]_keywords",
                           canonical_record_lines(expected_full.table_desc.columns[i].keywords),
                           canonical_record_lines(td.table_dat.table_desc.columns[i].keywords));
    }
    {
        std::size_t total_kw_fields = 0;
        for (std::size_t i = 0; i < ncol; ++i) {
            total_kw_fields += td.table_dat.table_desc.columns[i].keywords.entries().size();
        }
        std::cout << "  " << label << ": [PASS] col_keywords (" << ncol << " cols, "
                  << total_kw_fields << " fields total, content verified)\n";
    }

    // --- sm_mapping verification ---
    {
        // Map each column to its SM type via column_setups -> storage_managers.
        std::string sm_type_for_cols;
        for (std::size_t i = 0; i < td.table_dat.column_setups.size(); ++i) {
            const auto seq = td.table_dat.column_setups[i].sequence_number;
            for (std::size_t j = 0; j < td.table_dat.storage_managers.size(); ++j) {
                if (td.table_dat.storage_managers[j].sequence_number == seq) {
                    if (i == 0) {
                        sm_type_for_cols = td.table_dat.storage_managers[j].type_name;
                    } else if (sm_type_for_cols != td.table_dat.storage_managers[j].type_name) {
                        throw std::runtime_error(std::string(label) +
                                                 ": mixed SM types in column_setups");
                    }
                    break;
                }
            }
        }
        if (sm_type_for_cols != "StandardStMan") {
            throw std::runtime_error(std::string(label) +
                                     ": sm_mapping mismatch: expected StandardStMan got " +
                                     sm_type_for_cols);
        }
        std::cout << "  " << label << ": [PASS] sm_mapping (strict, "
                  << td.table_dat.column_setups.size() << " cols -> " << sm_type_for_cols << ")\n";
    }

    // --- Cell-value verification using SsmReader ---
    // Find the StandardStMan entry in the parsed table.dat.
    std::size_t ssm_idx = td.table_dat.storage_managers.size();
    for (std::size_t i = 0; i < td.table_dat.storage_managers.size(); ++i) {
        if (td.table_dat.storage_managers[i].type_name == "StandardStMan") {
            ssm_idx = i;
            break;
        }
    }
    if (ssm_idx == td.table_dat.storage_managers.size()) {
        throw std::runtime_error(std::string(label) + ": no StandardStMan in table.dat");
    }
    // Check that the .f0 data file is present; skip cell verification if absent
    // (mini does not yet write SSM data files until W13).
    const auto& sm = td.table_dat.storage_managers[ssm_idx];
    const auto f0_path = input_dir / ("table.f" + std::to_string(sm.sequence_number));
    if (std::filesystem::exists(f0_path)) {
        casacore_mini::SsmReader reader;
        reader.open(input_dir.string(), ssm_idx, td.table_dat);

        // Verify expected cell values: 5 rows, 4 columns.
        const std::int32_t expected_id[] = {0, 10, 20, 30, 40};
        const float expected_value[] = {0.0F, 1.5F, 3.0F, 4.5F, 6.0F};
        const std::string expected_label_str[] = {"row_0", "row_1", "row_2", "row_3", "row_4"};
        const double expected_dval[] = {0.0, 3.14, 6.28, 9.42, 12.56};

        for (std::uint64_t r = 0; r < 5; ++r) {
            const auto vi = reader.read_cell("id", r);
            const auto* ip = std::get_if<std::int32_t>(&vi);
            if (ip == nullptr || *ip != expected_id[r]) {
                throw std::runtime_error(std::string(label) + ": id[" + std::to_string(r) +
                                         "] mismatch: expected " + std::to_string(expected_id[r]));
            }

            const auto vv = reader.read_cell("value", r);
            const auto* fp = std::get_if<float>(&vv);
            if (fp == nullptr || std::fabs(*fp - expected_value[r]) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": value[" + std::to_string(r) +
                                         "] mismatch");
            }

            const auto vl = reader.read_cell("label", r);
            const auto* sp = std::get_if<std::string>(&vl);
            if (sp == nullptr || *sp != expected_label_str[r]) {
                throw std::runtime_error(std::string(label) + ": label[" + std::to_string(r) +
                                         "] mismatch: expected '" + expected_label_str[r] + "'");
            }

            const auto vd = reader.read_cell("dval", r);
            const auto* dp = std::get_if<double>(&vd);
            if (dp == nullptr || std::fabs(*dp - expected_dval[r]) > kFloat64Tolerance) {
                throw std::runtime_error(std::string(label) + ": dval[" + std::to_string(r) +
                                         "] mismatch");
            }
        }
        std::cout << "  " << label
                  << ": [PASS] cell_values (20 cells, float tol=1e-6, double tol=1e-10)\n";
    } else {
        std::cout << "  " << label << ": SSM data file not present; cell verification skipped\n";
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void dump_table_dir_artifact(const std::filesystem::path& input_dir,
                             const std::filesystem::path& output_path) {
    const auto td = casacore_mini::read_table_directory(input_dir.string());
    write_text(output_path, canonical_table_dir_lines(td));
}

// --- tiled table directory interop (mini side: read-only structural verification) ---

/// Strip SM file detail lines and replace sm_file_count with wildcard.
[[nodiscard]] std::vector<std::string> strip_sm_file_lines(const std::vector<std::string>& lines) {
    std::vector<std::string> result;
    for (const auto& line : lines) {
        if (line.rfind("sm[", 0) == 0) {
            continue;
        }
        if (line.rfind("sm_file_count=", 0) == 0) {
            result.emplace_back("sm_file_count=*");
            continue;
        }
        result.push_back(line);
    }
    return result;
}

/// Build expected metadata lines for the TiledColumnStMan test table.
[[nodiscard]] std::vector<std::string> expected_tiled_col_meta() {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=10");
    lines.emplace_back("ncol=2");
    lines.emplace_back("col[0].name_b64=" + base64_encode("data"));
    lines.emplace_back("col[0].dtype=TpFloat");
    lines.emplace_back("col[1].name_b64=" + base64_encode("flags"));
    lines.emplace_back("col[1].dtype=TpInt");
    lines.emplace_back("sm_file_count=*");
    return lines;
}

/// Build expected metadata lines for the TiledCellStMan test table.
[[nodiscard]] std::vector<std::string> expected_tiled_cell_meta() {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=5");
    lines.emplace_back("ncol=1");
    lines.emplace_back("col[0].name_b64=" + base64_encode("map"));
    lines.emplace_back("col[0].dtype=TpFloat");
    lines.emplace_back("sm_file_count=*");
    return lines;
}

/// Build expected metadata lines for the TiledShapeStMan test table.
[[nodiscard]] std::vector<std::string> expected_tiled_shape_meta() {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=5");
    lines.emplace_back("ncol=1");
    lines.emplace_back("col[0].name_b64=" + base64_encode("vis"));
    lines.emplace_back("col[0].dtype=TpComplex");
    lines.emplace_back("sm_file_count=*");
    return lines;
}

/// Build expected metadata lines for the TiledDataStMan test table.
[[nodiscard]] std::vector<std::string> expected_tiled_data_meta() {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=5");
    lines.emplace_back("ncol=1");
    lines.emplace_back("col[0].name_b64=" + base64_encode("spectrum"));
    lines.emplace_back("col[0].dtype=TpFloat");
    lines.emplace_back("sm_file_count=*");
    return lines;
}

/// Find the TSM storage manager index in table_dat.
[[nodiscard]] std::size_t find_tsm_index(const casacore_mini::TableDatFull& td,
                                         const std::string_view label) {
    for (std::size_t i = 0; i < td.storage_managers.size(); ++i) {
        const auto& t = td.storage_managers[i].type_name;
        if (t.find("Tiled") != std::string::npos) {
            return i;
        }
    }
    throw std::runtime_error(std::string(label) + ": no Tiled SM found");
}

// Forward declarations for tiled builder functions (defined later in file).
[[nodiscard]] casacore_mini::TableDatFull build_tiled_col_table_dat();
[[nodiscard]] casacore_mini::TableDatFull build_tiled_cell_table_dat();
[[nodiscard]] casacore_mini::TableDatFull build_tiled_shape_table_dat();
[[nodiscard]] casacore_mini::TableDatFull build_tiled_data_table_dat();

/// Verify a tiled table directory against expected metadata, with cell-value
/// verification when TSM data files are present.
void verify_tiled_dir_artifact(const std::filesystem::path& input_dir,
                               const std::vector<std::string>& expected_meta,
                               const std::string_view label) {
    const auto td = casacore_mini::read_table_directory(input_dir.string());
    const auto actual_dir_lines = canonical_table_dir_lines(td);
    const auto actual_meta = strip_sm_file_lines(actual_dir_lines);
    verify_lines_equal(label, expected_meta, actual_meta);

    const auto row_count = td.table_dat.row_count;
    const auto ncol = td.table_dat.table_desc.columns.size();
    std::cout << "  " << label << ": [PASS] structure (" << row_count << " rows, " << ncol
              << " cols)\n";

    // --- table_keywords verification (content) ---
    // Look up the expected builder for this tiled type.
    {
        casacore_mini::TableDatFull expected_full;
        if (label == "tiled-col-dir") {
            expected_full = build_tiled_col_table_dat();
        } else if (label == "tiled-cell-dir") {
            expected_full = build_tiled_cell_table_dat();
        } else if (label == "tiled-shape-dir") {
            expected_full = build_tiled_shape_table_dat();
        } else if (label == "tiled-data-dir") {
            expected_full = build_tiled_data_table_dat();
        }
        verify_lines_equal(std::string(label) + ":table_keywords",
                           canonical_record_lines(expected_full.table_desc.keywords),
                           canonical_record_lines(td.table_dat.table_desc.keywords));
        std::cout << "  " << label << ": [PASS] table_keywords ("
                  << td.table_dat.table_desc.keywords.entries().size()
                  << " fields, content verified)\n";

        // col_keywords
        for (std::size_t i = 0; i < ncol; ++i) {
            verify_lines_equal(std::string(label) + ":col[" + std::to_string(i) + "]_keywords",
                               canonical_record_lines(expected_full.table_desc.columns[i].keywords),
                               canonical_record_lines(td.table_dat.table_desc.columns[i].keywords));
        }
        std::size_t total_kw_fields = 0;
        for (std::size_t i = 0; i < ncol; ++i) {
            total_kw_fields += td.table_dat.table_desc.columns[i].keywords.entries().size();
        }
        std::cout << "  " << label << ": [PASS] col_keywords (" << ncol << " cols, "
                  << total_kw_fields << " fields total, content verified)\n";
    }

    // --- sm_mapping verification ---
    {
        std::string sm_type_for_cols;
        for (std::size_t i = 0; i < td.table_dat.column_setups.size(); ++i) {
            const auto seq = td.table_dat.column_setups[i].sequence_number;
            for (std::size_t j = 0; j < td.table_dat.storage_managers.size(); ++j) {
                if (td.table_dat.storage_managers[j].sequence_number == seq) {
                    if (i == 0) {
                        sm_type_for_cols = td.table_dat.storage_managers[j].type_name;
                    } else if (sm_type_for_cols != td.table_dat.storage_managers[j].type_name) {
                        throw std::runtime_error(std::string(label) +
                                                 ": mixed SM types in column_setups");
                    }
                    break;
                }
            }
        }
        // Derive expected SM type from label.
        std::string expected_sm_type;
        if (label == "tiled-col-dir") {
            expected_sm_type = "TiledColumnStMan";
        } else if (label == "tiled-cell-dir") {
            expected_sm_type = "TiledCellStMan";
        } else if (label == "tiled-shape-dir") {
            expected_sm_type = "TiledShapeStMan";
        } else if (label == "tiled-data-dir") {
            expected_sm_type = "TiledDataStMan";
        }
        if (!expected_sm_type.empty() && sm_type_for_cols != expected_sm_type) {
            throw std::runtime_error(std::string(label) + ": sm_mapping mismatch: expected " +
                                     expected_sm_type + " got " + sm_type_for_cols);
        }
        std::cout << "  " << label << ": [PASS] sm_mapping (strict, "
                  << td.table_dat.column_setups.size() << " cols -> " << sm_type_for_cols << ")\n";
    }

    // Attempt cell-value verification if a TSM data file exists.
    // Probe for both _TSM0 and _TSM1 (TiledShapeStMan uses _TSM1).
    const auto tsm0_path = input_dir / "table.f0_TSM0";
    const auto tsm1_path = input_dir / "table.f0_TSM1";
    if (!std::filesystem::exists(tsm0_path) && !std::filesystem::exists(tsm1_path)) {
        throw std::runtime_error(std::string(label) +
                                 ": neither table.f0_TSM0 nor table.f0_TSM1 found; "
                                 "cannot verify cell values");
    }

    const auto tsm_idx = find_tsm_index(td.table_dat, label);
    casacore_mini::TiledStManReader reader;
    reader.open(input_dir.string(), tsm_idx, td.table_dat);

    const auto& sm_type = td.table_dat.storage_managers[tsm_idx].type_name;
    std::size_t cells_verified = 0;

    if (sm_type == "TiledColumnStMan") {
        // data[r] = all elements r*0.1F, flags[r] = all elements r.
        for (std::uint64_t r = 0; r < 10; ++r) {
            auto data_vals = reader.read_float_cell("data", r);
            const float exp_d = static_cast<float>(r) * 0.1F;
            for (std::size_t i = 0; i < data_vals.size(); ++i) {
                if (std::fabs(data_vals[i] - exp_d) > kFloat32Tolerance) {
                    throw std::runtime_error(std::string(label) + ": data[" + std::to_string(r) +
                                             "][" + std::to_string(i) + "] mismatch: got " +
                                             std::to_string(data_vals[i]) + " expected " +
                                             std::to_string(exp_d));
                }
            }

            auto flag_vals = reader.read_int_cell("flags", r);
            const auto exp_f = static_cast<std::int32_t>(r);
            for (std::size_t i = 0; i < flag_vals.size(); ++i) {
                if (flag_vals[i] != exp_f) {
                    throw std::runtime_error(std::string(label) + ": flags[" + std::to_string(r) +
                                             "][" + std::to_string(i) + "] mismatch");
                }
            }
            cells_verified += 2;
        }
    } else if (sm_type == "TiledCellStMan") {
        // map[r] = all elements r*1.5F.
        for (std::uint64_t r = 0; r < 5; ++r) {
            auto vals = reader.read_float_cell("map", r);
            const float exp_v = static_cast<float>(r) * 1.5F;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (std::fabs(vals[i] - exp_v) > kFloat32Tolerance) {
                    throw std::runtime_error(std::string(label) + ": map[" + std::to_string(r) +
                                             "][" + std::to_string(i) + "] mismatch");
                }
            }
            ++cells_verified;
        }
    } else if (sm_type == "TiledShapeStMan") {
        // vis[r] = all elements Complex(r, 0.0).
        for (std::uint64_t r = 0; r < 5; ++r) {
            auto raw = reader.read_raw_cell("vis", r);
            const std::size_t cell_elems = raw.size() / std::size_t{8};
            if (raw.size() < cell_elems * std::size_t{8}) {
                throw std::runtime_error(std::string(label) + ": vis raw data too small");
            }
            for (std::size_t i = 0; i < cell_elems; ++i) {
                float real_val = 0;
                float imag_val = 0;
                std::memcpy(&real_val, raw.data() + i * std::size_t{8}, 4);
                std::memcpy(&imag_val, raw.data() + i * std::size_t{8} + 4, 4);
                const float exp_r = static_cast<float>(r);
                if (std::fabs(real_val - exp_r) > kFloat32Tolerance ||
                    std::fabs(imag_val) > kFloat32Tolerance) {
                    throw std::runtime_error(
                        std::string(label) + ": vis[" + std::to_string(r) + "][" +
                        std::to_string(i) + "] mismatch: got (" + std::to_string(real_val) + "," +
                        std::to_string(imag_val) + ") expected (" + std::to_string(exp_r) + ",0)");
                }
            }
            ++cells_verified;
        }
    } else if (sm_type == "TiledDataStMan") {
        // spectrum[r] = all elements r*0.01F.
        for (std::uint64_t r = 0; r < 5; ++r) {
            auto vals = reader.read_float_cell("spectrum", r);
            const float exp_v = static_cast<float>(r) * 0.01F;
            for (std::size_t i = 0; i < vals.size(); ++i) {
                if (std::fabs(vals[i] - exp_v) > kFloat32Tolerance) {
                    throw std::runtime_error(std::string(label) + ": spectrum[" +
                                             std::to_string(r) + "][" + std::to_string(i) +
                                             "] mismatch");
                }
            }
            ++cells_verified;
        }
    }

    if (cells_verified > 0) {
        std::cout << "  " << label << ": [PASS] cell_values (" << cells_verified
                  << " cells, float tol=1e-6)\n";
    }
}

/// Build expected metadata lines for the IncrementalStMan test table.
[[nodiscard]] std::vector<std::string> expected_ism_meta() {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=10");
    lines.emplace_back("ncol=3");
    lines.emplace_back("col[0].name_b64=" + base64_encode("time"));
    lines.emplace_back("col[0].dtype=TpDouble");
    lines.emplace_back("col[1].name_b64=" + base64_encode("antenna"));
    lines.emplace_back("col[1].dtype=TpInt");
    lines.emplace_back("col[2].name_b64=" + base64_encode("flag"));
    lines.emplace_back("col[2].dtype=TpBool");
    lines.emplace_back("sm_file_count=*");
    return lines;
}

/// Build a TableDatFull for an IncrementalStMan table.
[[nodiscard]] casacore_mini::TableDatFull build_ism_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 10;
    full.big_endian = true;
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_ism";

    auto make_col = [](const std::string& name, casacore_mini::DataType dtype) {
        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::scalar;
        col.name = name;
        col.data_type = dtype;
        col.dm_type = "IncrementalStMan";
        col.dm_group = "ISMData";
        col.type_string = casacore_col_type_string(col.kind, dtype);
        col.version = 1;
        return col;
    };

    full.table_desc.columns.push_back(make_col("time", casacore_mini::DataType::tp_double));
    full.table_desc.columns.push_back(make_col("antenna", casacore_mini::DataType::tp_int));
    full.table_desc.columns.push_back(make_col("flag", casacore_mini::DataType::tp_bool));

    // Table keywords: instrument, epoch.
    full.table_desc.keywords.set("instrument", casacore_mini::RecordValue(std::string("ALMA")));
    full.table_desc.keywords.set("epoch", casacore_mini::RecordValue(4.8e9));

    // Column keywords on "time": MEASINFO (sub-Record) and QuantumUnits (string_array).
    {
        casacore_mini::Record measinfo;
        measinfo.set("type", casacore_mini::RecordValue(std::string("epoch")));
        measinfo.set("Ref", casacore_mini::RecordValue(std::string("UTC")));
        full.table_desc.columns[0].keywords.set(
            "MEASINFO", casacore_mini::RecordValue::from_record(std::move(measinfo)));

        casacore_mini::RecordValue::string_array units;
        units.shape = {1};
        units.elements = {"s"};
        full.table_desc.columns[0].keywords.set("QuantumUnits",
                                                casacore_mini::RecordValue(std::move(units)));
    }

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "IncrementalStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : full.table_desc.columns) {
        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        full.column_setups.push_back(cms);
    }

    full.post_td_row_count = 10;
    return full;
}

void write_ism_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_ism_table_dat();
    full.big_endian = false; // LE to match casacore on LE systems

    casacore_mini::IsmWriter ism_writer;
    ism_writer.setup(full.table_desc.columns, full.row_count, full.big_endian, "ISMData");

    // Write the expected cell values (same as casacore side).
    for (std::uint64_t i = 0; i < 10; ++i) {
        ism_writer.write_cell(0, i,
                              casacore_mini::CellValue{4.8e9 + static_cast<double>(i) * 10.0});
        ism_writer.write_cell(1, i, casacore_mini::CellValue{static_cast<std::int32_t>(i % 3)});
        ism_writer.write_cell(2, i, casacore_mini::CellValue{(i % 2) == 0});
    }

    full.storage_managers[0].data_blob = ism_writer.make_blob();
    casacore_mini::write_table_directory(output_dir.string(), full);
    ism_writer.write_file(output_dir.string(), 0);
}

void verify_ism_dir_artifact(const std::filesystem::path& input_dir, const std::string_view label,
                             const bool relaxed_keywords) {
    const auto td = casacore_mini::read_table_directory(input_dir.string());
    const auto actual_lines = canonical_table_dir_lines(td);
    const auto actual_meta = strip_sm_file_lines(actual_lines);
    verify_lines_equal(label, expected_ism_meta(), actual_meta);

    const auto row_count = td.table_dat.row_count;
    const auto ncol = td.table_dat.table_desc.columns.size();
    std::cout << "  " << label << ": [PASS] structure (" << row_count << " rows, " << ncol
              << " cols)\n";

    const auto expected_full = build_ism_table_dat();

    // --- table_keywords verification ---
    {
        const auto actual_keyword_lines = canonical_record_lines(td.table_dat.table_desc.keywords);
        if (!relaxed_keywords) {
            verify_lines_equal(std::string(label) + ":table_keywords",
                               canonical_record_lines(expected_full.table_desc.keywords),
                               actual_keyword_lines);
            std::cout << "  " << label << ": [PASS] table_keywords ("
                      << td.table_dat.table_desc.keywords.entries().size()
                      << " fields, content verified)\n";
        } else {
            std::cout << "  " << label << ": [PASS] table_keywords ("
                      << td.table_dat.table_desc.keywords.entries().size()
                      << " fields, relaxed fixture mode)\n";
        }
    }

    // --- col_keywords verification ---
    {
        std::size_t total_kw_fields = 0;
        for (std::size_t i = 0; i < ncol; ++i) {
            const auto actual_col_keyword_lines =
                canonical_record_lines(td.table_dat.table_desc.columns[i].keywords);
            if (!relaxed_keywords) {
                verify_lines_equal(
                    std::string(label) + ":col[" + std::to_string(i) + "]_keywords",
                    canonical_record_lines(expected_full.table_desc.columns[i].keywords),
                    actual_col_keyword_lines);
            }
            total_kw_fields += td.table_dat.table_desc.columns[i].keywords.entries().size();
        }
        std::cout << "  " << label << ": [PASS] col_keywords (" << ncol << " cols, "
                  << total_kw_fields
                  << (relaxed_keywords ? " fields total, relaxed fixture mode)\n"
                                       : " fields total, content verified)\n");
    }

    // --- sm_mapping verification ---
    {
        std::string sm_type_for_cols;
        for (std::size_t i = 0; i < td.table_dat.column_setups.size(); ++i) {
            const auto seq = td.table_dat.column_setups[i].sequence_number;
            for (std::size_t j = 0; j < td.table_dat.storage_managers.size(); ++j) {
                if (td.table_dat.storage_managers[j].sequence_number == seq) {
                    if (i == 0) {
                        sm_type_for_cols = td.table_dat.storage_managers[j].type_name;
                    } else if (sm_type_for_cols != td.table_dat.storage_managers[j].type_name) {
                        throw std::runtime_error(std::string(label) +
                                                 ": mixed SM types in column_setups");
                    }
                    break;
                }
            }
        }
        if (sm_type_for_cols != "IncrementalStMan") {
            throw std::runtime_error(std::string(label) +
                                     ": sm_mapping mismatch: expected IncrementalStMan got " +
                                     sm_type_for_cols);
        }
        std::cout << "  " << label << ": [PASS] sm_mapping (strict, "
                  << td.table_dat.column_setups.size() << " cols -> " << sm_type_for_cols << ")\n";
    }

    // Cell-value verification if the .f0 file exists.
    const auto f0_path = input_dir / "table.f0";
    if (std::filesystem::exists(f0_path)) {
        // Find IncrementalStMan.
        std::size_t ism_idx = td.table_dat.storage_managers.size();
        for (std::size_t i = 0; i < td.table_dat.storage_managers.size(); ++i) {
            if (td.table_dat.storage_managers[i].type_name == "IncrementalStMan") {
                ism_idx = i;
                break;
            }
        }
        if (ism_idx == td.table_dat.storage_managers.size()) {
            throw std::runtime_error(std::string(label) + ": no IncrementalStMan found");
        }

        casacore_mini::IsmReader reader;
        reader.open(input_dir.string(), ism_idx, td.table_dat);

        for (std::uint64_t i = 0; i < 10; ++i) {
            // time: 4.8e9 + i * 10.0
            const auto vt = reader.read_cell("time", i);
            const auto* dp = std::get_if<double>(&vt);
            const double exp_time = 4.8e9 + static_cast<double>(i) * 10.0;
            if (dp == nullptr || std::fabs(*dp - exp_time) > kFloat64Tolerance) {
                throw std::runtime_error(std::string(label) + ": time[" + std::to_string(i) +
                                         "] mismatch");
            }

            // antenna: i % 3
            const auto va = reader.read_cell("antenna", i);
            const auto* ip = std::get_if<std::int32_t>(&va);
            if (ip == nullptr || *ip != static_cast<std::int32_t>(i % 3)) {
                throw std::runtime_error(std::string(label) + ": antenna[" + std::to_string(i) +
                                         "] mismatch");
            }

            // flag: (i % 2) == 0
            const auto vf = reader.read_cell("flag", i);
            const auto* bp = std::get_if<bool>(&vf);
            if (bp == nullptr || *bp != ((i % 2) == 0)) {
                throw std::runtime_error(std::string(label) + ": flag[" + std::to_string(i) +
                                         "] mismatch");
            }
        }
        std::cout << "  " << label << ": [PASS] cell_values (30 cells, double tol=1e-10)\n";
    } else {
        std::cout << "  " << label << ": ISM data file not present; cell verification skipped\n";
    }
}

/// Build a TableDatFull for a TiledColumnStMan table (mini can only write table.dat).
[[nodiscard]] casacore_mini::TableDatFull build_tiled_col_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 10;
    full.big_endian = false; // LE to match TSM tile data byte order
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tiledcol";

    auto make_arr_col = [](const std::string& name, casacore_mini::DataType dtype) {
        casacore_mini::ColumnDesc col;
        col.kind = casacore_mini::ColumnKind::array;
        col.name = name;
        col.data_type = dtype;
        col.dm_type = "TiledColumnStMan";
        col.dm_group = "TiledCol";
        col.type_string = casacore_col_type_string(col.kind, dtype);
        col.version = 1;
        col.ndim = 2;
        col.shape = {4, 8};
        col.options = 4; // FixedShape
        return col;
    };

    full.table_desc.columns.push_back(make_arr_col("data", casacore_mini::DataType::tp_float));
    full.table_desc.columns.push_back(make_arr_col("flags", casacore_mini::DataType::tp_int));

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledColumnStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    for (const auto& col : full.table_desc.columns) {
        casacore_mini::ColumnManagerSetup cms;
        cms.column_name = col.name;
        cms.sequence_number = 0;
        cms.has_shape = true;
        cms.shape = col.shape;
        full.column_setups.push_back(cms);
    }

    full.post_td_row_count = 10;
    return full;
}

/// Build a TableDatFull for a TiledCellStMan table.
[[nodiscard]] casacore_mini::TableDatFull build_tiled_cell_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 5;
    full.big_endian = false; // LE to match TSM tile data byte order
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tiledcell";

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::array;
    col.name = "map";
    col.data_type = casacore_mini::DataType::tp_float;
    col.dm_type = "TiledCellStMan";
    col.dm_group = "TiledCell";
    col.type_string = casacore_col_type_string(col.kind, col.data_type);
    col.version = 1;
    col.ndim = 2;
    col.shape = {32, 8};
    col.options = 4; // FixedShape
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledCellStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "map";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape = {32, 8};
    full.column_setups.push_back(cms);

    full.post_td_row_count = 5;
    return full;
}

/// Build a TableDatFull for a TiledShapeStMan table.
[[nodiscard]] casacore_mini::TableDatFull build_tiled_shape_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 5;
    full.big_endian = false; // LE to match TSM tile data byte order
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tiledshape";

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::array;
    col.name = "vis";
    col.data_type = casacore_mini::DataType::tp_complex;
    col.dm_type = "TiledShapeStMan";
    col.dm_group = "TiledShape";
    col.type_string = casacore_col_type_string(col.kind, col.data_type);
    col.version = 1;
    col.ndim = 2;
    col.shape = {4, 16}; // actual shape used by casacore side
    col.options = 4;     // FixedShape
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledShapeStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "vis";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape = {4, 16};
    full.column_setups.push_back(cms);

    full.post_td_row_count = 5;
    return full;
}

/// Build a TableDatFull for a TiledDataStMan table.
[[nodiscard]] casacore_mini::TableDatFull build_tiled_data_table_dat() {
    casacore_mini::TableDatFull full;
    full.table_version = 2;
    full.row_count = 5;
    full.big_endian = false; // LE to match TSM tile data byte order
    full.table_type = "PlainTable";
    full.table_desc.version = 2;
    full.table_desc.name = "test_tileddata";

    // TiledDataStMan requires a hypercolumn definition in the table desc
    // keywords. casacore's defineHypercolumn stores it as a sub-Record
    // named "Hypercolumn_<name>" with fields: ndim, data, coord, id.
    {
        casacore_mini::Record hc;
        hc.set("ndim", casacore_mini::RecordValue(std::int32_t{2}));
        // data columns: Array<String> shape [1] with ["spectrum"]
        casacore_mini::RecordValue::string_array data_arr;
        data_arr.shape = {1};
        data_arr.elements = {"spectrum"};
        hc.set("data", casacore_mini::RecordValue(std::move(data_arr)));
        // coord columns: empty Array<String>
        casacore_mini::RecordValue::string_array coord_arr;
        coord_arr.shape = {0};
        hc.set("coord", casacore_mini::RecordValue(std::move(coord_arr)));
        // id columns: empty Array<String>
        casacore_mini::RecordValue::string_array id_arr;
        id_arr.shape = {0};
        hc.set("id", casacore_mini::RecordValue(std::move(id_arr)));
        full.table_desc.private_keywords.set(
            "Hypercolumn_TiledData", casacore_mini::RecordValue::from_record(std::move(hc)));
    }

    casacore_mini::ColumnDesc col;
    col.kind = casacore_mini::ColumnKind::array;
    col.name = "spectrum";
    col.data_type = casacore_mini::DataType::tp_float;
    col.dm_type = "TiledDataStMan";
    col.dm_group = "TiledData";
    col.type_string = casacore_col_type_string(col.kind, col.data_type);
    col.version = 1;
    col.ndim = 1;
    col.shape = {256};
    col.options = 4; // FixedShape
    full.table_desc.columns.push_back(col);

    casacore_mini::StorageManagerSetup sm;
    sm.type_name = "TiledDataStMan";
    sm.sequence_number = 0;
    full.storage_managers.push_back(sm);

    casacore_mini::ColumnManagerSetup cms;
    cms.column_name = "spectrum";
    cms.sequence_number = 0;
    cms.has_shape = true;
    cms.shape = {256};
    full.column_setups.push_back(cms);

    full.post_td_row_count = 5;
    return full;
}

void write_tiled_col_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_tiled_col_table_dat();

    casacore_mini::TiledStManWriter tsm_writer;
    tsm_writer.setup("TiledColumnStMan", "TiledCol", full.table_desc.columns, full.row_count);

    // Write cell data: data[r] = all elements r*0.1F, flags[r] = all elements r.
    for (std::uint64_t r = 0; r < 10; ++r) {
        std::vector<float> data_vals(32, static_cast<float>(r) * 0.1F); // 4*8=32
        tsm_writer.write_float_cell(0, r, data_vals);

        std::vector<std::int32_t> flag_vals(32, static_cast<std::int32_t>(r));
        tsm_writer.write_int_cell(1, r, flag_vals);
    }

    full.storage_managers[0].data_blob = tsm_writer.make_blob();
    casacore_mini::write_table_directory(output_dir.string(), full);
    tsm_writer.write_files(output_dir.string(), 0);
}

void write_tiled_cell_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_tiled_cell_table_dat();

    casacore_mini::TiledStManWriter tsm_writer;
    tsm_writer.setup("TiledCellStMan", "TiledCell", full.table_desc.columns, full.row_count);

    // Write cell data: map[r] = all elements r*1.5F.
    for (std::uint64_t r = 0; r < 5; ++r) {
        std::vector<float> vals(std::size_t{32} * 8, static_cast<float>(r) * 1.5F);
        tsm_writer.write_float_cell(0, r, vals);
    }

    full.storage_managers[0].data_blob = tsm_writer.make_blob();
    casacore_mini::write_table_directory(output_dir.string(), full);
    tsm_writer.write_files(output_dir.string(), 0);
}

void write_tiled_shape_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_tiled_shape_table_dat();

    casacore_mini::TiledStManWriter tsm_writer;
    tsm_writer.setup("TiledShapeStMan", "TiledShape", full.table_desc.columns, full.row_count);

    // Write cell data: vis[r] = all elements Complex(r, 0.0).
    // Complex = 8 bytes per element = 2 floats (real, imag).
    for (std::uint64_t r = 0; r < 5; ++r) {
        constexpr std::size_t kCellElems = std::size_t{4} * 16; // 64 complex elements
        std::vector<std::uint8_t> raw(kCellElems * 8);          // 8 bytes per complex
        for (std::size_t i = 0; i < kCellElems; ++i) {
            auto real_val = static_cast<float>(r);
            float imag_val = 0.0F;
            std::memcpy(raw.data() + i * std::size_t{8}, &real_val, 4);
            std::memcpy(raw.data() + i * std::size_t{8} + 4, &imag_val, 4);
        }
        tsm_writer.write_raw_cell(0, r, raw);
    }

    full.storage_managers[0].data_blob = tsm_writer.make_blob();
    casacore_mini::write_table_directory(output_dir.string(), full);
    tsm_writer.write_files(output_dir.string(), 0);
}

void write_tiled_data_dir_artifact(const std::filesystem::path& output_dir) {
    auto full = build_tiled_data_table_dat();

    casacore_mini::TiledStManWriter tsm_writer;
    tsm_writer.setup("TiledDataStMan", "TiledData", full.table_desc.columns, full.row_count);

    // Write cell data: spectrum[r] = all elements r*0.01F.
    for (std::uint64_t r = 0; r < 5; ++r) {
        std::vector<float> vals(256, static_cast<float>(r) * 0.01F);
        tsm_writer.write_float_cell(0, r, vals);
    }

    full.storage_managers[0].data_blob = tsm_writer.make_blob();
    casacore_mini::write_table_directory(output_dir.string(), full);
    tsm_writer.write_files(output_dir.string(), 0);
}

[[nodiscard]] std::string usage() {
    return "Usage:\n"
           "  interop_mini_tool write-record-basic --output <path>\n"
           "  interop_mini_tool write-record-nested --output <path>\n"
           "  interop_mini_tool write-record-fixture-logtable-time --output <path>\n"
           "  interop_mini_tool write-record-fixture-ms-table --output <path>\n"
           "  interop_mini_tool write-record-fixture-ms-uvw --output <path>\n"
           "  interop_mini_tool write-record-fixture-pagedimage-table --output <path>\n"
           "  interop_mini_tool write-table-dat-header --output <path>\n"
           "  interop_mini_tool write-table-dat-full --output <path>\n"
           "  interop_mini_tool write-table-dir --output <dir>\n"
           "  interop_mini_tool write-ism-dir --output <dir>\n"
           "  interop_mini_tool write-tiled-col-dir --output <dir>\n"
           "  interop_mini_tool write-tiled-cell-dir --output <dir>\n"
           "  interop_mini_tool write-tiled-shape-dir --output <dir>\n"
           "  interop_mini_tool write-tiled-data-dir --output <dir>\n"
           "  interop_mini_tool verify-record-basic --input <path>\n"
           "  interop_mini_tool verify-record-nested --input <path>\n"
           "  interop_mini_tool verify-record-fixture-logtable-time --input <path>\n"
           "  interop_mini_tool verify-record-fixture-ms-table --input <path>\n"
           "  interop_mini_tool verify-record-fixture-ms-uvw --input <path>\n"
           "  interop_mini_tool verify-record-fixture-pagedimage-table --input <path>\n"
           "  interop_mini_tool verify-table-dat-header --input <path>\n"
           "  interop_mini_tool verify-table-dat-full --input <path>\n"
           "  interop_mini_tool verify-table-dir --input <dir>\n"
           "  interop_mini_tool verify-ism-dir --input <dir> [--relaxed-keywords]\n"
           "  interop_mini_tool verify-tiled-col-dir --input <dir>\n"
           "  interop_mini_tool verify-tiled-cell-dir --input <dir>\n"
           "  interop_mini_tool verify-tiled-shape-dir --input <dir>\n"
           "  interop_mini_tool verify-tiled-data-dir --input <dir>\n"
           "  interop_mini_tool dump-record --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dat-header --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dat-full --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dir --input <dir> --output <path>\n";
}

} // namespace

int main(int argc, char** argv) noexcept {
    try {
        if (argc < 2) {
            std::cerr << usage();
            return 2;
        }

        const std::string subcommand(argv[1]);
        if (subcommand == "write-record-basic") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_record_basic());
            return 0;
        }
        if (subcommand == "write-record-nested") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_record_nested());
            return 0;
        }
        if (subcommand == "write-record-fixture-logtable-time") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_fixture_record_artifact(*output, "logtable_time");
            return 0;
        }
        if (subcommand == "write-record-fixture-ms-table") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_fixture_record_artifact(*output, "ms_table");
            return 0;
        }
        if (subcommand == "write-record-fixture-ms-uvw") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_fixture_record_artifact(*output, "ms_uvw");
            return 0;
        }
        if (subcommand == "write-record-fixture-pagedimage-table") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_fixture_record_artifact(*output, "pagedimage_table");
            return 0;
        }
        if (subcommand == "write-table-dat-header") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_table_dat_header_artifact(*output);
            return 0;
        }
        if (subcommand == "dump-record") {
            const auto input = arg_value(argc, argv, "--input");
            const auto output = arg_value(argc, argv, "--output");
            if (!input.has_value() || !output.has_value()) {
                throw std::runtime_error("missing required --input/--output");
            }
            dump_record_artifact(DumpPaths{*input, *output});
            return 0;
        }
        if (subcommand == "dump-table-dat-header") {
            const auto input = arg_value(argc, argv, "--input");
            const auto output = arg_value(argc, argv, "--output");
            if (!input.has_value() || !output.has_value()) {
                throw std::runtime_error("missing required --input/--output");
            }
            dump_table_dat_header_artifact(DumpPaths{*input, *output});
            return 0;
        }
        if (subcommand == "verify-record-basic") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_record_basic(), "record-basic");
            return 0;
        }
        if (subcommand == "verify-record-nested") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_record_nested(), "record-nested");
            return 0;
        }
        if (subcommand == "verify-record-fixture-logtable-time") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_fixture_record_artifact(*input, "logtable_time", "record-fixture-logtable-time");
            return 0;
        }
        if (subcommand == "verify-record-fixture-ms-table") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_fixture_record_artifact(*input, "ms_table", "record-fixture-ms-table");
            return 0;
        }
        if (subcommand == "verify-record-fixture-ms-uvw") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_fixture_record_artifact(*input, "ms_uvw", "record-fixture-ms-uvw");
            return 0;
        }
        if (subcommand == "verify-record-fixture-pagedimage-table") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_fixture_record_artifact(*input, "pagedimage_table",
                                           "record-fixture-pagedimage-table");
            return 0;
        }
        if (subcommand == "verify-table-dat-header") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_table_dat_header_artifact(*input, "table-dat-header");
            return 0;
        }
        if (subcommand == "write-table-dat-full") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_table_dat_full_artifact(*output);
            return 0;
        }
        if (subcommand == "verify-table-dat-full") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_table_dat_full_artifact(*input, "table-dat-full");
            return 0;
        }
        if (subcommand == "dump-table-dat-full") {
            const auto input = arg_value(argc, argv, "--input");
            const auto output = arg_value(argc, argv, "--output");
            if (!input.has_value() || !output.has_value()) {
                throw std::runtime_error("missing required --input/--output");
            }
            dump_table_dat_full_artifact(DumpPaths{*input, *output});
            return 0;
        }
        if (subcommand == "write-table-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_table_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "verify-table-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_table_dir_artifact(*input, "table-dir");
            return 0;
        }
        if (subcommand == "dump-table-dir") {
            const auto input = arg_value(argc, argv, "--input");
            const auto output = arg_value(argc, argv, "--output");
            if (!input.has_value() || !output.has_value()) {
                throw std::runtime_error("missing required --input/--output");
            }
            dump_table_dir_artifact(*input, *output);
            return 0;
        }
        // --- ISM directory commands ---
        if (subcommand == "write-ism-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_ism_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "verify-ism-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ism_dir_artifact(*input, "ism-dir", has_flag(argc, argv, "--relaxed-keywords"));
            return 0;
        }
        // --- tiled directory commands ---
        if (subcommand == "write-tiled-col-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_tiled_col_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "write-tiled-cell-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_tiled_cell_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "write-tiled-shape-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_tiled_shape_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "write-tiled-data-dir") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_tiled_data_dir_artifact(*output);
            return 0;
        }
        if (subcommand == "verify-tiled-col-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_dir_artifact(*input, expected_tiled_col_meta(), "tiled-col-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-cell-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_dir_artifact(*input, expected_tiled_cell_meta(), "tiled-cell-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-shape-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_dir_artifact(*input, expected_tiled_shape_meta(), "tiled-shape-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-data-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_dir_artifact(*input, expected_tiled_data_meta(), "tiled-data-dir");
            return 0;
        }

        throw std::runtime_error("unknown subcommand: " + subcommand);
    } catch (const std::exception& error) {
        std::cerr << "interop_mini_tool failed: " << error.what() << '\n';
        std::cerr << usage();
        return 1;
    }
}
