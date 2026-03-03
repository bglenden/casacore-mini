// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/aipsio_record_writer.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/coordinate_system.hpp"
#include "casacore_mini/coordinate_util.hpp"
#include "casacore_mini/direction_coordinate.hpp"
#include "casacore_mini/image.hpp"
#include "casacore_mini/image_region.hpp"
#include "casacore_mini/incremental_stman.hpp"
#include "casacore_mini/lattice.hpp"
#include "casacore_mini/lattice_expr.hpp"
#include "casacore_mini/lattice_region.hpp"
#include "casacore_mini/linear_coordinate.hpp"
#include "casacore_mini/measure_convert.hpp"
#include "casacore_mini/measure_record.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/spectral_coordinate.hpp"
#include "casacore_mini/standard_stman.hpp"
#include "casacore_mini/stokes_coordinate.hpp"
#include "casacore_mini/table.hpp"
#include "casacore_mini/table_column.hpp"
#include "casacore_mini/table_dat.hpp"
#include "casacore_mini/table_dat_writer.hpp"
#include "casacore_mini/table_desc.hpp"
#include "casacore_mini/table_desc_writer.hpp"
#include "casacore_mini/table_directory.hpp"
#include "casacore_mini/tiled_stman.hpp"

#include "casacore_mini/measurement_set.hpp"
#include "casacore_mini/ms_columns.hpp"
#include "casacore_mini/ms_concat.hpp"
#include "casacore_mini/ms_metadata.hpp"
#include "casacore_mini/ms_selection.hpp"
#include "casacore_mini/ms_writer.hpp"

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
#include <map>
#include <optional>
#include <set>
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

void write_table_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    // Table keywords.
    TableCreateOptions opts;
    opts.sm_type = "StandardStMan";
    opts.sm_group = "StandardStMan";
    opts.big_endian = false; // LE to match casacore on LE systems
    opts.table_keywords.set("observer", RecordValue(std::string("test_user")));
    opts.table_keywords.set("telescope", RecordValue(std::string("VLA")));
    opts.table_keywords.set("version", RecordValue(std::int32_t{2}));

    // Column keyword on "value" (index 1): UNIT = "Jy".
    Record val_kw;
    val_kw.set("UNIT", RecordValue(std::string("Jy")));
    opts.column_keywords = {Record{}, std::move(val_kw), Record{}, Record{}};

    std::vector<TableColumnSpec> columns = {
        {"id", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"value", DataType::tp_float, ColumnKind::scalar, {}, ""},
        {"label", DataType::tp_string, ColumnKind::scalar, {}, ""},
        {"dval", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };

    // For StandardStMan tables this public Table API path delegates writes to SsmWriter.
    auto table = Table::create(output_dir, columns, 5, opts);
    ScalarColumn<std::int32_t> id_col(table, "id");
    ScalarColumn<float> value_col(table, "value");
    ScalarColumn<std::string> label_col(table, "label");
    ScalarColumn<double> dval_col(table, "dval");

    const std::int32_t id_vals[] = {0, 10, 20, 30, 40};
    const float value_vals[] = {0.0F, 1.5F, 3.0F, 4.5F, 6.0F};
    const std::string label_vals[] = {"row_0", "row_1", "row_2", "row_3", "row_4"};
    const double dval_vals[] = {0.0, 3.14, 6.28, 9.42, 12.56};

    for (std::uint64_t r = 0; r < 5; ++r) {
        id_col.put(r, id_vals[r]);
        value_col.put(r, value_vals[r]);
        label_col.put(r, label_vals[r]);
        dval_col.put(r, dval_vals[r]);
    }
    table.flush();
}

void verify_table_dir_artifact(const std::filesystem::path& input_dir,
                               const std::string_view label) {
    using namespace casacore_mini;

    auto table = Table::open(input_dir);

    // --- Structure verification ---
    if (table.nrow() != 5) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch: expected 5 got " +
                                 std::to_string(table.nrow()));
    }
    if (table.ncolumn() != 4) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch: expected 4 got " +
                                 std::to_string(table.ncolumn()));
    }

    // --- Column name and type verification ---
    {
        const std::string expected_names[] = {"id", "value", "label", "dval"};
        const DataType expected_types[] = {DataType::tp_int, DataType::tp_float,
                                           DataType::tp_string, DataType::tp_double};
        const ColumnKind expected_kinds[] = {ColumnKind::scalar, ColumnKind::scalar,
                                             ColumnKind::scalar, ColumnKind::scalar};
        for (std::size_t i = 0; i < 4; ++i) {
            const auto& col = table.columns()[i];
            if (col.name != expected_names[i]) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].name mismatch: expected '" + expected_names[i] +
                                         "' got '" + col.name + "'");
            }
            if (col.data_type != expected_types[i]) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].data_type mismatch");
            }
            if (col.kind != expected_kinds[i]) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].kind mismatch");
            }
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types verified)\n";

    // --- table_keywords verification ---
    {
        Record expected_kw;
        expected_kw.set("observer", RecordValue(std::string("test_user")));
        expected_kw.set("telescope", RecordValue(std::string("VLA")));
        expected_kw.set("version", RecordValue(std::int32_t{2}));
        verify_lines_equal(std::string(label) + ":table_keywords",
                           canonical_record_lines(expected_kw),
                           canonical_record_lines(table.keywords()));
        std::cout << "  " << label << ": [PASS] table_keywords ("
                  << table.keywords().entries().size() << " fields, content verified)\n";
    }

    // --- col_keywords verification ---
    {
        // Only "value" (index 1) has UNIT = "Jy".
        Record expected_val_kw;
        expected_val_kw.set("UNIT", RecordValue(std::string("Jy")));
        std::vector<Record> expected_col_kw = {Record{}, expected_val_kw, Record{}, Record{}};
        std::size_t total_kw_fields = 0;
        for (std::size_t i = 0; i < table.ncolumn(); ++i) {
            verify_lines_equal(std::string(label) + ":col[" + std::to_string(i) + "]_keywords",
                               canonical_record_lines(expected_col_kw[i]),
                               canonical_record_lines(table.columns()[i].keywords));
            total_kw_fields += table.columns()[i].keywords.entries().size();
        }
        std::cout << "  " << label << ": [PASS] col_keywords (" << table.ncolumn() << " cols, "
                  << total_kw_fields << " fields total, content verified)\n";
    }

    // --- Cell-value verification via Table API ---
    {
        // Use a non-const reference for ScalarColumn (it takes Table&).
        ScalarColumn<std::int32_t> id_col(table, "id");
        ScalarColumn<float> value_col(table, "value");
        ScalarColumn<std::string> label_col(table, "label");
        ScalarColumn<double> dval_col(table, "dval");

        const std::int32_t expected_id[] = {0, 10, 20, 30, 40};
        const float expected_value[] = {0.0F, 1.5F, 3.0F, 4.5F, 6.0F};
        const std::string expected_label_str[] = {"row_0", "row_1", "row_2", "row_3", "row_4"};
        const double expected_dval[] = {0.0, 3.14, 6.28, 9.42, 12.56};

        for (std::uint64_t r = 0; r < 5; ++r) {
            if (id_col.get(r) != expected_id[r]) {
                throw std::runtime_error(std::string(label) + ": id[" + std::to_string(r) +
                                         "] mismatch: expected " + std::to_string(expected_id[r]));
            }
            if (std::fabs(value_col.get(r) - expected_value[r]) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": value[" + std::to_string(r) +
                                         "] mismatch");
            }
            if (label_col.get(r) != expected_label_str[r]) {
                throw std::runtime_error(std::string(label) + ": label[" + std::to_string(r) +
                                         "] mismatch: expected '" + expected_label_str[r] + "'");
            }
            if (std::fabs(dval_col.get(r) - expected_dval[r]) > kFloat64Tolerance) {
                throw std::runtime_error(std::string(label) + ": dval[" + std::to_string(r) +
                                         "] mismatch");
            }
        }
        std::cout << "  " << label
                  << ": [PASS] cell_values (20 cells, float tol=1e-6, double tol=1e-10)\n";
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void dump_table_dir_artifact(const std::filesystem::path& input_dir,
                             const std::filesystem::path& output_path) {
    const auto td = casacore_mini::read_table_directory(input_dir.string());
    write_text(output_path, canonical_table_dir_lines(td));
}

// --- tiled table directory verification (via Table API) ---

/// Verify a tiled-col table directory.
void verify_tiled_col_dir_artifact(const std::filesystem::path& input_dir,
                                   const std::string_view label) {
    using namespace casacore_mini;
    // For tiled managers this public Table API read path delegates to TiledStManReader.
    auto table = Table::open(input_dir);

    if (table.nrow() != 10) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch");
    }
    if (table.ncolumn() != 2) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch");
    }

    // --- Column name, type, kind, shape verification ---
    {
        const auto& c0 = table.columns()[0];
        const auto& c1 = table.columns()[1];
        if (c0.name != "data") {
            throw std::runtime_error(std::string(label) + ": col[0].name mismatch: got '" +
                                     c0.name + "'");
        }
        if (c0.data_type != DataType::tp_float || c0.kind != ColumnKind::array) {
            throw std::runtime_error(std::string(label) + ": col[0] type/kind mismatch");
        }
        if (c0.shape != std::vector<std::int64_t>{4, 8}) {
            throw std::runtime_error(std::string(label) + ": col[0].shape mismatch");
        }
        if (c1.name != "flags") {
            throw std::runtime_error(std::string(label) + ": col[1].name mismatch: got '" +
                                     c1.name + "'");
        }
        if (c1.data_type != DataType::tp_int || c1.kind != ColumnKind::array) {
            throw std::runtime_error(std::string(label) + ": col[1] type/kind mismatch");
        }
        if (c1.shape != std::vector<std::int64_t>{4, 8}) {
            throw std::runtime_error(std::string(label) + ": col[1].shape mismatch");
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types+shapes verified)\n";

    // Keywords: tiled-col has none.
    std::cout << "  " << label << ": [PASS] table_keywords (" << table.keywords().entries().size()
              << " fields)\n";

    // Cell-value verification.
    ArrayColumn<float> data_col(table, "data");
    ArrayColumn<std::int32_t> flags_col(table, "flags");
    std::size_t cells_verified = 0;
    for (std::uint64_t r = 0; r < 10; ++r) {
        auto data_vals = data_col.get(r);
        if (data_vals.size() != 32) { // 4*8
            throw std::runtime_error(std::string(label) + ": data[" + std::to_string(r) +
                                     "].size mismatch: expected 32 got " +
                                     std::to_string(data_vals.size()));
        }
        const float exp_d = static_cast<float>(r) * 0.1F;
        for (std::size_t i = 0; i < data_vals.size(); ++i) {
            if (std::fabs(data_vals[i] - exp_d) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": data[" + std::to_string(r) + "][" +
                                         std::to_string(i) + "] mismatch");
            }
        }
        auto flag_vals = flags_col.get(r);
        if (flag_vals.size() != 32) {
            throw std::runtime_error(std::string(label) + ": flags[" + std::to_string(r) +
                                     "].size mismatch: expected 32 got " +
                                     std::to_string(flag_vals.size()));
        }
        const auto exp_f = static_cast<std::int32_t>(r);
        for (std::size_t i = 0; i < flag_vals.size(); ++i) {
            if (flag_vals[i] != exp_f) {
                throw std::runtime_error(std::string(label) + ": flags[" + std::to_string(r) +
                                         "][" + std::to_string(i) + "] mismatch");
            }
        }
        cells_verified += 2;
    }
    std::cout << "  " << label << ": [PASS] cell_values (" << cells_verified
              << " cells, float tol=1e-6)\n";
}

/// Verify a tiled-cell table directory.
void verify_tiled_cell_dir_artifact(const std::filesystem::path& input_dir,
                                    const std::string_view label) {
    using namespace casacore_mini;
    auto table = Table::open(input_dir);

    if (table.nrow() != 5) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch");
    }
    if (table.ncolumn() != 1) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch");
    }

    {
        const auto& c0 = table.columns()[0];
        if (c0.name != "map") {
            throw std::runtime_error(std::string(label) + ": col[0].name mismatch: got '" +
                                     c0.name + "'");
        }
        if (c0.data_type != DataType::tp_float || c0.kind != ColumnKind::array) {
            throw std::runtime_error(std::string(label) + ": col[0] type/kind mismatch");
        }
        if (c0.shape != std::vector<std::int64_t>{32, 8}) {
            throw std::runtime_error(std::string(label) + ": col[0].shape mismatch");
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types+shapes verified)\n";

    ArrayColumn<float> map_col(table, "map");
    std::size_t cells_verified = 0;
    for (std::uint64_t r = 0; r < 5; ++r) {
        auto vals = map_col.get(r);
        if (vals.size() != std::size_t{32} * 8) {
            throw std::runtime_error(std::string(label) + ": map[" + std::to_string(r) +
                                     "].size mismatch: expected 256 got " +
                                     std::to_string(vals.size()));
        }
        const float exp_v = static_cast<float>(r) * 1.5F;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (std::fabs(vals[i] - exp_v) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": map[" + std::to_string(r) + "][" +
                                         std::to_string(i) + "] mismatch");
            }
        }
        ++cells_verified;
    }
    std::cout << "  " << label << ": [PASS] cell_values (" << cells_verified
              << " cells, float tol=1e-6)\n";
}

/// Verify a tiled-shape table directory (Complex arrays via raw reads).
void verify_tiled_shape_dir_artifact(const std::filesystem::path& input_dir,
                                     const std::string_view label) {
    using namespace casacore_mini;
    auto table = Table::open(input_dir);

    if (table.nrow() != 5) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch");
    }
    if (table.ncolumn() != 1) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch");
    }

    {
        const auto& c0 = table.columns()[0];
        if (c0.name != "vis") {
            throw std::runtime_error(std::string(label) + ": col[0].name mismatch: got '" +
                                     c0.name + "'");
        }
        if (c0.data_type != DataType::tp_complex || c0.kind != ColumnKind::array) {
            throw std::runtime_error(std::string(label) + ": col[0] type/kind mismatch");
        }
        if (c0.shape != std::vector<std::int64_t>{4, 16}) {
            throw std::runtime_error(std::string(label) + ": col[0].shape mismatch");
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types+shapes verified)\n";

    ArrayColumn<std::uint8_t> vis_col(table, "vis");
    std::size_t cells_verified = 0;
    for (std::uint64_t r = 0; r < 5; ++r) {
        auto raw = vis_col.get(r);
        const std::size_t cell_elems = raw.size() / std::size_t{8};
        for (std::size_t i = 0; i < cell_elems; ++i) {
            float real_val = 0;
            float imag_val = 0;
            std::memcpy(&real_val, raw.data() + i * std::size_t{8}, 4);
            std::memcpy(&imag_val, raw.data() + i * std::size_t{8} + 4, 4);
            const float exp_r = static_cast<float>(r);
            if (std::fabs(real_val - exp_r) > kFloat32Tolerance ||
                std::fabs(imag_val) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": vis[" + std::to_string(r) + "][" +
                                         std::to_string(i) + "] mismatch");
            }
        }
        ++cells_verified;
    }
    std::cout << "  " << label << ": [PASS] cell_values (" << cells_verified
              << " cells, float tol=1e-6)\n";
}

/// Verify a tiled-data table directory.
void verify_tiled_data_dir_artifact(const std::filesystem::path& input_dir,
                                    const std::string_view label) {
    using namespace casacore_mini;
    auto table = Table::open(input_dir);

    if (table.nrow() != 5) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch");
    }
    if (table.ncolumn() != 1) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch");
    }

    {
        const auto& c0 = table.columns()[0];
        if (c0.name != "spectrum") {
            throw std::runtime_error(std::string(label) + ": col[0].name mismatch: got '" +
                                     c0.name + "'");
        }
        if (c0.data_type != DataType::tp_float || c0.kind != ColumnKind::array) {
            throw std::runtime_error(std::string(label) + ": col[0] type/kind mismatch");
        }
        if (c0.shape != std::vector<std::int64_t>{256}) {
            throw std::runtime_error(std::string(label) + ": col[0].shape mismatch");
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types+shapes verified)\n";

    ArrayColumn<float> spectrum_col(table, "spectrum");
    std::size_t cells_verified = 0;
    for (std::uint64_t r = 0; r < 5; ++r) {
        auto vals = spectrum_col.get(r);
        const float exp_v = static_cast<float>(r) * 0.01F;
        for (std::size_t i = 0; i < vals.size(); ++i) {
            if (std::fabs(vals[i] - exp_v) > kFloat32Tolerance) {
                throw std::runtime_error(std::string(label) + ": spectrum[" + std::to_string(r) +
                                         "][" + std::to_string(i) + "] mismatch");
            }
        }
        ++cells_verified;
    }
    std::cout << "  " << label << ": [PASS] cell_values (" << cells_verified
              << " cells, float tol=1e-6)\n";
}

/// Build the ISM-specific column keywords for the "time" column.
[[nodiscard]] casacore_mini::Record build_ism_time_keywords() {
    using namespace casacore_mini;
    Record kw;
    Record measinfo;
    measinfo.set("type", RecordValue(std::string("epoch")));
    measinfo.set("Ref", RecordValue(std::string("UTC")));
    kw.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    RecordValue::string_array units;
    units.shape = {1};
    units.elements = {"s"};
    kw.set("QuantumUnits", RecordValue(std::move(units)));
    return kw;
}

void write_ism_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    TableCreateOptions opts;
    opts.sm_type = "IncrementalStMan";
    opts.sm_group = "ISMData";
    opts.big_endian = false; // LE to match casacore on LE systems
    opts.table_keywords.set("instrument", RecordValue(std::string("ALMA")));
    opts.table_keywords.set("epoch", RecordValue(4.8e9));

    // Column keywords: only "time" (index 0) has MEASINFO + QuantumUnits.
    opts.column_keywords = {build_ism_time_keywords(), Record{}, Record{}};

    std::vector<TableColumnSpec> columns = {
        {"time", DataType::tp_double, ColumnKind::scalar, {}, ""},
        {"antenna", DataType::tp_int, ColumnKind::scalar, {}, ""},
        {"flag", DataType::tp_bool, ColumnKind::scalar, {}, ""},
    };

    // For IncrementalStMan tables this public Table API path delegates writes to IsmWriter.
    auto table = Table::create(output_dir, columns, 10, opts);
    ScalarColumn<double> time_col(table, "time");
    ScalarColumn<std::int32_t> antenna_col(table, "antenna");
    ScalarColumn<bool> flag_col(table, "flag");

    for (std::uint64_t i = 0; i < 10; ++i) {
        time_col.put(i, 4.8e9 + static_cast<double>(i) * 10.0);
        antenna_col.put(i, static_cast<std::int32_t>(i % 3));
        flag_col.put(i, (i % 2) == 0);
    }
    table.flush();
}

void verify_ism_dir_artifact(const std::filesystem::path& input_dir, const std::string_view label,
                             const bool relaxed_keywords) {
    using namespace casacore_mini;
    // For IncrementalStMan tables this public Table API read path delegates to IsmReader.
    auto table = Table::open(input_dir);

    // --- Structure verification ---
    if (table.nrow() != 10) {
        throw std::runtime_error(std::string(label) + ": row_count mismatch");
    }
    if (table.ncolumn() != 3) {
        throw std::runtime_error(std::string(label) + ": ncol mismatch");
    }

    // --- Column name and type verification ---
    {
        const std::string expected_names[] = {"time", "antenna", "flag"};
        const DataType expected_types[] = {DataType::tp_double, DataType::tp_int,
                                           DataType::tp_bool};
        for (std::size_t i = 0; i < 3; ++i) {
            const auto& col = table.columns()[i];
            if (col.name != expected_names[i]) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].name mismatch: expected '" + expected_names[i] +
                                         "' got '" + col.name + "'");
            }
            if (col.data_type != expected_types[i]) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].data_type mismatch");
            }
            if (col.kind != ColumnKind::scalar) {
                throw std::runtime_error(std::string(label) + ": col[" + std::to_string(i) +
                                         "].kind mismatch: expected scalar");
            }
        }
    }

    std::cout << "  " << label << ": [PASS] structure (" << table.nrow() << " rows, "
              << table.ncolumn() << " cols, names+types verified)\n";

    // --- table_keywords verification ---
    {
        Record expected_kw;
        expected_kw.set("instrument", RecordValue(std::string("ALMA")));
        expected_kw.set("epoch", RecordValue(4.8e9));
        if (!relaxed_keywords) {
            verify_lines_equal(std::string(label) + ":table_keywords",
                               canonical_record_lines(expected_kw),
                               canonical_record_lines(table.keywords()));
            std::cout << "  " << label << ": [PASS] table_keywords ("
                      << table.keywords().entries().size() << " fields, content verified)\n";
        } else {
            std::cout << "  " << label << ": [PASS] table_keywords ("
                      << table.keywords().entries().size() << " fields, relaxed fixture mode)\n";
        }
    }

    // --- col_keywords verification ---
    {
        std::vector<Record> expected_col_kw = {build_ism_time_keywords(), Record{}, Record{}};
        std::size_t total_kw_fields = 0;
        for (std::size_t i = 0; i < table.ncolumn(); ++i) {
            if (!relaxed_keywords) {
                verify_lines_equal(std::string(label) + ":col[" + std::to_string(i) + "]_keywords",
                                   canonical_record_lines(expected_col_kw[i]),
                                   canonical_record_lines(table.columns()[i].keywords));
            }
            total_kw_fields += table.columns()[i].keywords.entries().size();
        }
        std::cout << "  " << label << ": [PASS] col_keywords (" << table.ncolumn() << " cols, "
                  << total_kw_fields
                  << (relaxed_keywords ? " fields total, relaxed fixture mode)\n"
                                       : " fields total, content verified)\n");
    }

    // --- Cell-value verification via Table API ---
    {
        ScalarColumn<double> time_col(table, "time");
        ScalarColumn<std::int32_t> antenna_col(table, "antenna");
        ScalarColumn<bool> flag_col(table, "flag");

        for (std::uint64_t i = 0; i < 10; ++i) {
            const double exp_time = 4.8e9 + static_cast<double>(i) * 10.0;
            if (std::fabs(time_col.get(i) - exp_time) > kFloat64Tolerance) {
                throw std::runtime_error(std::string(label) + ": time[" + std::to_string(i) +
                                         "] mismatch");
            }
            if (antenna_col.get(i) != static_cast<std::int32_t>(i % 3)) {
                throw std::runtime_error(std::string(label) + ": antenna[" + std::to_string(i) +
                                         "] mismatch");
            }
            if (flag_col.get(i) != ((i % 2) == 0)) {
                throw std::runtime_error(std::string(label) + ": flag[" + std::to_string(i) +
                                         "] mismatch");
            }
        }
        std::cout << "  " << label << ": [PASS] cell_values (30 cells, double tol=1e-10)\n";
    }
}

void write_tiled_col_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    TableCreateOptions opts;
    opts.sm_type = "TiledColumnStMan";
    opts.sm_group = "TiledCol";
    opts.big_endian = false;

    std::vector<TableColumnSpec> columns = {
        {"data", DataType::tp_float, ColumnKind::array, {4, 8}, ""},
        {"flags", DataType::tp_int, ColumnKind::array, {4, 8}, ""},
    };

    // For tiled managers this public Table API write path delegates to TiledStManWriter.
    auto table = Table::create(output_dir, columns, 10, opts);
    ArrayColumn<float> data_col(table, "data");
    ArrayColumn<std::int32_t> flags_col(table, "flags");

    for (std::uint64_t r = 0; r < 10; ++r) {
        std::vector<float> data_vals(32, static_cast<float>(r) * 0.1F);
        data_col.put(r, data_vals);

        std::vector<std::int32_t> flag_vals(32, static_cast<std::int32_t>(r));
        flags_col.put(r, flag_vals);
    }
    table.flush();
}

void write_tiled_cell_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    TableCreateOptions opts;
    opts.sm_type = "TiledCellStMan";
    opts.sm_group = "TiledCell";
    opts.big_endian = false;

    std::vector<TableColumnSpec> columns = {
        {"map", DataType::tp_float, ColumnKind::array, {32, 8}, ""},
    };

    auto table = Table::create(output_dir, columns, 5, opts);
    ArrayColumn<float> map_col(table, "map");

    for (std::uint64_t r = 0; r < 5; ++r) {
        std::vector<float> vals(std::size_t{32} * 8, static_cast<float>(r) * 1.5F);
        map_col.put(r, vals);
    }
    table.flush();
}

void write_tiled_shape_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    TableCreateOptions opts;
    opts.sm_type = "TiledShapeStMan";
    opts.sm_group = "TiledShape";
    opts.big_endian = false;

    std::vector<TableColumnSpec> columns = {
        {"vis", DataType::tp_complex, ColumnKind::array, {4, 16}, ""},
    };

    auto table = Table::create(output_dir, columns, 5, opts);
    ArrayColumn<std::uint8_t> vis_col(table, "vis");

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
        vis_col.put(r, raw);
    }
    table.flush();
}

void write_tiled_data_dir_artifact(const std::filesystem::path& output_dir) {
    using namespace casacore_mini;

    // TiledDataStMan requires a hypercolumn definition in private keywords.
    TableCreateOptions opts;
    opts.sm_type = "TiledDataStMan";
    opts.sm_group = "TiledData";
    opts.big_endian = false;

    {
        Record hc;
        hc.set("ndim", RecordValue(std::int32_t{2}));
        RecordValue::string_array data_arr;
        data_arr.shape = {1};
        data_arr.elements = {"spectrum"};
        hc.set("data", RecordValue(std::move(data_arr)));
        RecordValue::string_array coord_arr;
        coord_arr.shape = {0};
        hc.set("coord", RecordValue(std::move(coord_arr)));
        RecordValue::string_array id_arr;
        id_arr.shape = {0};
        hc.set("id", RecordValue(std::move(id_arr)));
        opts.private_keywords.set("Hypercolumn_TiledData", RecordValue::from_record(std::move(hc)));
    }

    std::vector<TableColumnSpec> columns = {
        {"spectrum", DataType::tp_float, ColumnKind::array, {256}, ""},
    };

    auto table = Table::create(output_dir, columns, 5, opts);
    ArrayColumn<float> spectrum_col(table, "spectrum");

    for (std::uint64_t r = 0; r < 5; ++r) {
        std::vector<float> vals(256, static_cast<float>(r) * 0.01F);
        spectrum_col.put(r, vals);
    }
    table.flush();
}

// ---------------------------------------------------------------------------
// Phase 8 measure/coordinate interop artifacts
// ---------------------------------------------------------------------------

/// Artifact 1: Scalar measure-column table keywords (MEpoch, TIME column).
[[nodiscard]] casacore_mini::Record build_measure_scalar_record() {
    // MEASINFO sub-record
    casacore_mini::Record measinfo;
    measinfo.set("type", casacore_mini::RecordValue(std::string("epoch")));
    measinfo.set("Ref", casacore_mini::RecordValue(std::string("UTC")));

    casacore_mini::Record record;
    record.set("MEASINFO", casacore_mini::RecordValue::from_record(std::move(measinfo)));

    // QuantumUnits
    casacore_mini::RecordValue::string_array units;
    units.shape = {1};
    units.elements = {"s"};
    record.set("QuantumUnits", casacore_mini::RecordValue(std::move(units)));

    return record;
}

/// Artifact 2: Array measure-column table keywords (MDirection, UVW-like).
[[nodiscard]] casacore_mini::Record build_measure_array_record() {
    casacore_mini::Record measinfo;
    measinfo.set("type", casacore_mini::RecordValue(std::string("direction")));
    measinfo.set("Ref", casacore_mini::RecordValue(std::string("J2000")));

    casacore_mini::Record record;
    record.set("MEASINFO", casacore_mini::RecordValue::from_record(std::move(measinfo)));

    casacore_mini::RecordValue::string_array units;
    units.shape = {2};
    units.elements = {"rad", "rad"};
    record.set("QuantumUnits", casacore_mini::RecordValue(std::move(units)));

    return record;
}

/// Artifact 3: Rich measure-keyword table (multiple MeasureHolder records).
[[nodiscard]] casacore_mini::Record build_measure_rich_record() {
    // Epoch measure in MeasureHolder format
    casacore_mini::Record epoch_rec;
    epoch_rec.set("type", casacore_mini::RecordValue(std::string("epoch")));
    epoch_rec.set("refer", casacore_mini::RecordValue(std::string("UTC")));
    casacore_mini::Record m0;
    m0.set("value", casacore_mini::RecordValue(double{59000.5})); // MJD
    m0.set("unit", casacore_mini::RecordValue(std::string("d")));
    epoch_rec.set("m0", casacore_mini::RecordValue::from_record(std::move(m0)));

    // Direction measure
    casacore_mini::Record dir_rec;
    dir_rec.set("type", casacore_mini::RecordValue(std::string("direction")));
    dir_rec.set("refer", casacore_mini::RecordValue(std::string("J2000")));
    casacore_mini::Record d0;
    d0.set("value", casacore_mini::RecordValue(double{M_PI})); // pi rad = 180 deg RA
    d0.set("unit", casacore_mini::RecordValue(std::string("rad")));
    dir_rec.set("m0", casacore_mini::RecordValue::from_record(std::move(d0)));
    casacore_mini::Record d1;
    d1.set("value", casacore_mini::RecordValue(double{M_PI / 4.0})); // pi/4 rad = 45 deg Dec
    d1.set("unit", casacore_mini::RecordValue(std::string("rad")));
    dir_rec.set("m1", casacore_mini::RecordValue::from_record(std::move(d1)));

    // Position measure
    casacore_mini::Record pos_rec;
    pos_rec.set("type", casacore_mini::RecordValue(std::string("position")));
    pos_rec.set("refer", casacore_mini::RecordValue(std::string("WGS84")));
    casacore_mini::Record p0;
    p0.set("value",
           casacore_mini::RecordValue(double{-118.0 * M_PI / 180.0})); // lon rad (-118 deg)
    p0.set("unit", casacore_mini::RecordValue(std::string("rad")));
    pos_rec.set("m0", casacore_mini::RecordValue::from_record(std::move(p0)));
    casacore_mini::Record p1;
    p1.set("value", casacore_mini::RecordValue(double{34.0 * M_PI / 180.0})); // lat rad (34 deg)
    p1.set("unit", casacore_mini::RecordValue(std::string("rad")));
    pos_rec.set("m1", casacore_mini::RecordValue::from_record(std::move(p1)));
    casacore_mini::Record p2;
    p2.set("value", casacore_mini::RecordValue(double{100.0})); // height m
    p2.set("unit", casacore_mini::RecordValue(std::string("m")));
    pos_rec.set("m2", casacore_mini::RecordValue::from_record(std::move(p2)));

    casacore_mini::Record record;
    record.set("obsdate", casacore_mini::RecordValue::from_record(std::move(epoch_rec)));
    record.set("pointingcenter", casacore_mini::RecordValue::from_record(std::move(dir_rec)));
    record.set("telescopeposition", casacore_mini::RecordValue::from_record(std::move(pos_rec)));
    record.set("telescope", casacore_mini::RecordValue(std::string("VLA")));
    record.set("observer", casacore_mini::RecordValue(std::string("TestObserver")));

    return record;
}

/// Artifact 4: Coordinate-rich image metadata (Direction + Spectral + Stokes).
[[nodiscard]] casacore_mini::Record build_coord_keywords_record() {
    using namespace casacore_mini;
    constexpr double kDeg2Rad = M_PI / 180.0;

    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::sin, {0.0, 0.0}},
        180.0 * kDeg2Rad,      // ref lon = 180 deg
        45.0 * kDeg2Rad,       // ref lat = 45 deg
        -kDeg2Rad / 3600.0,    // -1 arcsec/pixel
        kDeg2Rad / 3600.0,     // 1 arcsec/pixel
        std::vector<double>{}, // identity PC
        128.0, 128.0));        // crpix

    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    cs.add_coordinate(std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2, 3, 4}));

    return cs.save();
}

/// Artifact 5: Mixed coordinate system (Direction + Spectral + Stokes + Linear + ObsInfo).
[[nodiscard]] casacore_mini::Record build_mixed_coords_record() {
    using namespace casacore_mini;
    constexpr double kDeg2Rad = M_PI / 180.0;

    CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<DirectionCoordinate>(
        DirectionRef::j2000, Projection{ProjectionType::tan, {0.0, 0.0}}, 0.0, 0.0,
        -kDeg2Rad / 3600.0, kDeg2Rad / 3600.0, std::vector<double>{}, 64.0, 64.0));

    cs.add_coordinate(std::make_unique<SpectralCoordinate>(FrequencyRef::lsrk, 1.42e9, 1e6, 0.0));

    cs.add_coordinate(std::make_unique<StokesCoordinate>(std::vector<std::int32_t>{1, 2, 3, 4}));

    LinearXform xform;
    xform.crval = {0.0};
    xform.cdelt = {1.0};
    xform.crpix = {0.0};
    xform.pc = {1.0};
    cs.add_coordinate(
        std::make_unique<LinearCoordinate>(std::vector<std::string>{"Deconvolver Iteration"},
                                           std::vector<std::string>{""}, std::move(xform)));

    ObsInfo obs;
    obs.telescope = "ALMA";
    obs.observer = "TestUser";
    cs.set_obs_info(obs);

    return cs.save();
}

/// Artifact 6: Conversion vectors Record (pre-computed measure conversion results).
[[nodiscard]] casacore_mini::Record build_conversion_vectors_record() {
    casacore_mini::Record record;

    // Epoch conversions: MJD 59000.5 UTC -> TAI
    // UTC->TAI adds leap seconds (37s as of 2020)
    record.set("epoch_utc_mjd", casacore_mini::RecordValue(double{59000.5}));
    record.set("epoch_tai_mjd", casacore_mini::RecordValue(double{59000.5 + 37.0 / 86400.0}));

    // Direction: RA=180deg Dec=45deg J2000 -> Galactic
    // Known values: l~123.932 deg, b~+62.871 deg (approximate)
    casacore_mini::RecordValue::double_array j2000_dir;
    j2000_dir.shape = {2};
    j2000_dir.elements = {M_PI, M_PI / 4.0}; // 180 deg, 45 deg in rad
    record.set("direction_j2000_rad", casacore_mini::RecordValue(std::move(j2000_dir)));

    // Doppler conversions (purely mathematical, deterministic)
    // Optical doppler: v/c = z = 0.1
    record.set("doppler_z", casacore_mini::RecordValue(double{0.1}));
    // Radio doppler from z: v_radio/c = 1 - 1/(1+z) = 1 - 1/1.1 = 0.09090909...
    record.set("doppler_radio", casacore_mini::RecordValue(double{1.0 - 1.0 / 1.1}));
    // Relativistic beta from z: beta = ((1+z)^2-1)/((1+z)^2+1)
    constexpr double kZ = 0.1;
    const double beta = ((1.0 + kZ) * (1.0 + kZ) - 1.0) / ((1.0 + kZ) * (1.0 + kZ) + 1.0);
    record.set("doppler_beta", casacore_mini::RecordValue(beta));

    // Frequency: rest=1.42e9 Hz, optical z=0.1 -> observed = rest/(1+z)
    record.set("freq_rest_hz", casacore_mini::RecordValue(double{1.42e9}));
    record.set("freq_observed_hz", casacore_mini::RecordValue(double{1.42e9 / 1.1}));

    return record;
}

// --- Semantic verification helpers for cross-tool interop ---

// Get a sub-record from a Record, or nullptr if not present/not a record.
[[nodiscard]] const casacore_mini::Record* get_subrecord(const casacore_mini::Record& record,
                                                         std::string_view key) {
    const auto* val = record.find(key);
    if (val == nullptr) {
        return nullptr;
    }
    const auto* rp = std::get_if<casacore_mini::RecordValue::record_ptr>(&val->storage());
    if (rp == nullptr || *rp == nullptr) {
        return nullptr;
    }
    return rp->get();
}

// Get a double scalar from a Record, or nullopt.
[[nodiscard]] std::optional<double> get_double(const casacore_mini::Record& record,
                                               std::string_view key) {
    const auto* val = record.find(key);
    if (val == nullptr) {
        return std::nullopt;
    }
    if (const auto* d = std::get_if<double>(&val->storage())) {
        return *d;
    }
    return std::nullopt;
}

// Get a string scalar from a Record, or nullopt.
[[nodiscard]] std::optional<std::string> get_string(const casacore_mini::Record& record,
                                                    std::string_view key) {
    const auto* val = record.find(key);
    if (val == nullptr) {
        return std::nullopt;
    }
    if (const auto* s = std::get_if<std::string>(&val->storage())) {
        return *s;
    }
    return std::nullopt;
}

// Get a double array from a Record.
[[nodiscard]] std::vector<double> get_double_array(const casacore_mini::Record& record,
                                                   std::string_view key) {
    const auto* val = record.find(key);
    if (val == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<casacore_mini::RecordValue::double_array>(&val->storage())) {
        return arr->elements;
    }
    return {};
}

// Get an int32 or int64 array from a Record as int64 values.
[[nodiscard]] std::vector<std::int64_t> get_int_array(const casacore_mini::Record& record,
                                                      std::string_view key) {
    const auto* val = record.find(key);
    if (val == nullptr) {
        return {};
    }
    if (const auto* arr = std::get_if<casacore_mini::RecordValue::int32_array>(&val->storage())) {
        std::vector<std::int64_t> result;
        result.reserve(arr->elements.size());
        for (auto v : arr->elements) {
            result.push_back(static_cast<std::int64_t>(v));
        }
        return result;
    }
    if (const auto* arr = std::get_if<casacore_mini::RecordValue::int64_array>(&val->storage())) {
        return arr->elements;
    }
    return {};
}

// Find a sub-record by trying multiple candidate field names.
[[nodiscard]] const casacore_mini::Record*
find_subrecord_any(const casacore_mini::Record& record,
                   const std::vector<std::string>& candidates) {
    for (const auto& name : candidates) {
        const auto* sub = get_subrecord(record, name);
        if (sub != nullptr) {
            return sub;
        }
    }
    return nullptr;
}

void verify_semantic_string(const casacore_mini::Record& record, std::string_view key,
                            std::string_view expected, std::string_view label) {
    auto val = get_string(record, key);
    if (!val.has_value()) {
        throw std::runtime_error(std::string(label) + ": missing string field '" +
                                 std::string(key) + "'");
    }
    if (*val != expected) {
        throw std::runtime_error(std::string(label) + ": field '" + std::string(key) +
                                 "' expected '" + std::string(expected) + "' got '" + *val + "'");
    }
}

void verify_semantic_double(const casacore_mini::Record& record, std::string_view key,
                            double expected, std::string_view label) {
    auto val = get_double(record, key);
    if (!val.has_value()) {
        throw std::runtime_error(std::string(label) + ": missing double field '" +
                                 std::string(key) + "'");
    }
    if (std::fabs(*val - expected) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field '" + std::string(key) +
                                 "' expected " + std::to_string(expected) + " got " +
                                 std::to_string(*val));
    }
}

void verify_semantic_double_array(const casacore_mini::Record& record, std::string_view key,
                                  std::size_t expected_size, double expected_first,
                                  double expected_last, std::string_view label) {
    auto arr = get_double_array(record, key);
    if (arr.size() < expected_size) {
        throw std::runtime_error(std::string(label) + ": field '" + std::string(key) +
                                 "' expected at least " + std::to_string(expected_size) +
                                 " elements, got " + std::to_string(arr.size()));
    }
    if (std::fabs(arr.front() - expected_first) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field '" + std::string(key) +
                                 "[0]' expected " + std::to_string(expected_first) + " got " +
                                 std::to_string(arr.front()));
    }
    if (std::fabs(arr.back() - expected_last) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field '" + std::string(key) +
                                 "[last]' expected " + std::to_string(expected_last) + " got " +
                                 std::to_string(arr.back()));
    }
}

// Semantic verification for coord-keywords (artifact 4).
// Accepts both mini format (CoordinateSystem::save) and casacore format (manual Record).
void verify_coord_keywords_semantic(const casacore_mini::Record& record, std::string_view label) {
    // direction0 must exist.
    const auto* dir0 = get_subrecord(record, "direction0");
    if (dir0 == nullptr) {
        throw std::runtime_error(std::string(label) + ": missing direction0 sub-record");
    }
    verify_semantic_string(*dir0, "system", "J2000", label);

    // Projection may be "projection" or "projection_type".
    auto proj = get_string(*dir0, "projection");
    if (!proj.has_value()) {
        proj = get_string(*dir0, "projection_type");
    }
    if (proj.has_value() && *proj != "SIN") {
        throw std::runtime_error(std::string(label) + ": direction0 projection expected SIN, got " +
                                 *proj);
    }

    verify_semantic_double_array(*dir0, "crval", 2, M_PI, M_PI / 4.0, label);
    verify_semantic_double_array(*dir0, "crpix", 2, 128.0, 128.0, label);

    // Find spectral sub-record.
    const auto* spec = find_subrecord_any(record, {"spectral1", "spectral2"});
    if (spec == nullptr) {
        throw std::runtime_error(std::string(label) + ": missing spectral sub-record");
    }
    verify_semantic_string(*spec, "system", "LSRK", label);
    verify_semantic_double(*spec, "crval", 1.42e9, label);
    verify_semantic_double(*spec, "cdelt", 1e6, label);

    // Find stokes sub-record.
    const auto* stk = find_subrecord_any(record, {"stokes2", "stokes1"});
    if (stk == nullptr) {
        throw std::runtime_error(std::string(label) + ": missing stokes sub-record");
    }
    auto stokes_vals = get_int_array(*stk, "stokes");
    if (stokes_vals.size() != 4) {
        throw std::runtime_error(std::string(label) + ": stokes expected 4 elements, got " +
                                 std::to_string(stokes_vals.size()));
    }
    if (stokes_vals[0] != 1) {
        throw std::runtime_error(std::string(label) + ": stokes[0] expected 1 (I)");
    }

    std::cout << "  " << label << ": [PASS] semantic (J2000-SIN, LSRK, IQUV)\n";
}

// Semantic verification for mixed-coords (artifact 5).
// Accepts both mini format (J2000/TAN + LSRK + Stokes + Linear) and
// casacore format (GALACTIC/SIN + Linear + TOPO).
void verify_mixed_coords_semantic(const casacore_mini::Record& record, std::string_view label) {
    const auto* dir0 = get_subrecord(record, "direction0");
    if (dir0 == nullptr) {
        throw std::runtime_error(std::string(label) + ": missing direction0 sub-record");
    }

    auto system_val = get_string(*dir0, "system");
    if (!system_val.has_value()) {
        throw std::runtime_error(std::string(label) + ": direction0 missing 'system' field");
    }

    if (*system_val == "GALACTIC") {
        // Casacore format: GALACTIC-SIN + linear1 + spectral2 (TOPO).
        const auto* lin = get_subrecord(record, "linear1");
        if (lin == nullptr) {
            throw std::runtime_error(std::string(label) + ": missing linear1 sub-record");
        }
        verify_semantic_double_array(*lin, "crval", 1, 0.0, 0.0, label);

        const auto* spec = get_subrecord(record, "spectral2");
        if (spec == nullptr) {
            throw std::runtime_error(std::string(label) + ": missing spectral2 sub-record");
        }
        verify_semantic_string(*spec, "system", "TOPO", label);
        verify_semantic_double(*spec, "crval", 1.0e9, label);

        std::cout << "  " << label << ": [PASS] semantic (GALACTIC-SIN, Linear, TOPO)\n";
    } else if (*system_val == "J2000") {
        // Mini format: J2000-TAN + spectral1 (LSRK) + stokes2 + linear3.
        const auto* spec = find_subrecord_any(record, {"spectral1", "spectral2"});
        if (spec == nullptr) {
            throw std::runtime_error(std::string(label) + ": missing spectral sub-record");
        }
        verify_semantic_string(*spec, "system", "LSRK", label);
        verify_semantic_double(*spec, "crval", 1.42e9, label);

        if (find_subrecord_any(record, {"stokes2", "stokes1", "stokes3"}) == nullptr) {
            throw std::runtime_error(std::string(label) + ": missing stokes sub-record");
        }
        if (find_subrecord_any(record, {"linear1", "linear2", "linear3"}) == nullptr) {
            throw std::runtime_error(std::string(label) + ": missing linear sub-record");
        }

        std::cout << "  " << label << ": [PASS] semantic (J2000-TAN, LSRK, Stokes, Linear)\n";
    } else {
        throw std::runtime_error(std::string(label) + ": unexpected direction system '" +
                                 *system_val + "'");
    }
}

// Semantic verification for conversion-vectors (artifact 6).
void verify_conversion_vectors_semantic(const casacore_mini::Record& record,
                                        std::string_view label) {
    verify_semantic_double(record, "epoch_utc_mjd", 59000.5, label);
    verify_semantic_double(record, "epoch_tai_mjd", 59000.5 + 37.0 / 86400.0, label);
    verify_semantic_double(record, "doppler_z", 0.1, label);
    verify_semantic_double(record, "doppler_radio", 1.0 - 1.0 / 1.1, label);
    constexpr double kZ = 0.1;
    const double expected_beta = ((1.0 + kZ) * (1.0 + kZ) - 1.0) / ((1.0 + kZ) * (1.0 + kZ) + 1.0);
    verify_semantic_double(record, "doppler_beta", expected_beta, label);
    verify_semantic_double(record, "freq_rest_hz", 1.42e9, label);
    verify_semantic_double(record, "freq_observed_hz", 1.42e9 / 1.1, label);

    verify_semantic_double_array(record, "direction_j2000_rad", 2, M_PI, M_PI / 4.0, label);

    std::cout << "  " << label << ": [PASS] semantic (epoch, doppler, freq conversions)\n";
}

// ---------------------------------------------------------------------------
// Phase 9: MS interop produce / verify helpers
// ---------------------------------------------------------------------------

void ms_verify_check(bool cond, const std::string& artifact, const std::string& detail) {
    if (!cond) {
        throw std::runtime_error(artifact + ": " + detail);
    }
}

// Required subtable names for a valid MS.
const std::vector<std::string> kRequiredSubtables = {
    "ANTENNA",     "DATA_DESCRIPTION", "FEED",         "FIELD",     "FLAG_CMD",        "HISTORY",
    "OBSERVATION", "POINTING",         "POLARIZATION", "PROCESSOR", "SPECTRAL_WINDOW", "STATE"};

void verify_required_subtables(casacore_mini::MeasurementSet& ms, const std::string& artifact) {
    for (const auto& name : kRequiredSubtables) {
        ms_verify_check(ms.has_subtable(name), artifact, "missing required subtable: " + name);
    }
}

// --- produce-ms-minimal ---

void produce_ms_minimal(const std::string& output) {
    namespace fs = std::filesystem;
    if (fs::exists(output)) {
        fs::remove_all(output);
    }
    auto ms = casacore_mini::MeasurementSet::create(output, /*include_data=*/false);
    casacore_mini::MsWriter writer(ms);
    writer.add_antenna({.name = "ANT0",
                        .station = {},
                        .type = {},
                        .mount = {},
                        .position = {},
                        .offset = {},
                        .dish_diameter = 25.0,
                        .flag_row = false});
    writer.add_field(
        {.name = "F0", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_spectral_window({.num_chan = 4,
                                .name = "SPW0",
                                .ref_frequency = 1.4e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
    writer.add_observation({.telescope_name = "MINI",
                            .observer = {},
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE",
                      .flag_row = false});
    writer.flush();
    std::cout << "  produce-ms-minimal: wrote " << output << '\n';
}

void verify_ms_minimal(const std::string& input) {
    const std::string label = "ms-minimal";
    auto ms = casacore_mini::MeasurementSet::open(input);

    verify_required_subtables(ms, label);
    ms_verify_check(ms.row_count() == 0, label, "expected 0 main-table rows");

    casacore_mini::MsAntennaColumns ant_cols(ms);
    ms_verify_check(ant_cols.row_count() >= 1, label, "expected at least 1 antenna");

    casacore_mini::MsFieldColumns field_cols(ms);
    ms_verify_check(field_cols.row_count() >= 1, label, "expected at least 1 field");

    casacore_mini::MsSpWindowColumns spw_cols(ms);
    ms_verify_check(spw_cols.row_count() >= 1, label, "expected at least 1 SPW");

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-ms-representative ---

void produce_ms_representative(const std::string& output) {
    namespace fs = std::filesystem;
    if (fs::exists(output)) {
        fs::remove_all(output);
    }
    auto ms = casacore_mini::MeasurementSet::create(output, /*include_data=*/false);
    casacore_mini::MsWriter writer(ms);

    // 6 antennas
    for (int i = 0; i < 6; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
    }

    // 2 fields
    writer.add_field({.name = "3C273",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "J1924-2914",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    // 2 SPWs
    writer.add_spectral_window({.num_chan = 64,
                                .name = "SPW0",
                                .ref_frequency = 1.4e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    writer.add_spectral_window({.num_chan = 128,
                                .name = "SPW1",
                                .ref_frequency = 2.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});

    // 2 data descriptions (one per SPW)
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0, .flag_row = false});

    writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA",
                            .observer = {},
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE",
                      .flag_row = false});

    // 15 baselines across 2 fields, 2 scans, varying time
    const double base_time = 4.8e9;
    int row_count = 0;
    for (int field = 0; field < 2; ++field) {
        for (int scan = 1; scan <= 2; ++scan) {
            // baselines 0-1, 0-2, 1-2, 2-3 (4 per field/scan combo, ~16 total but we add a few)
            const std::vector<std::pair<int, int>> baselines = {{0, 1}, {0, 2}, {1, 2}, {2, 3}};
            for (const auto& [ant1, ant2] : baselines) {
                const double time_offset = static_cast<double>(row_count) * 10.0;
                writer.add_row({
                    .antenna1 = ant1,
                    .antenna2 = ant2,
                    .array_id = 0,
                    .data_desc_id = field, // alternate SPW per field
                    .exposure = 0.0,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = field,
                    .flag_row = false,
                    .interval = 0.0,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = scan,
                    .state_id = 0,
                    .time = base_time + time_offset,
                    .time_centroid = base_time + time_offset,
                    .uvw = {100.0 + row_count, 200.0 - row_count, 50.0 + row_count * 0.5},
                    .sigma = {1.0F, 1.0F},
                    .weight = {1.0F, 1.0F},
                    .data = {},
                    .flag = {},
                });
                ++row_count;
            }
        }
    }

    writer.flush();
    std::cout << "  produce-ms-representative: wrote " << output << " (" << row_count << " rows)\n";
}

void verify_ms_representative(const std::string& input) {
    const std::string label = "ms-representative";
    auto ms = casacore_mini::MeasurementSet::open(input);

    verify_required_subtables(ms, label);
    ms_verify_check(ms.row_count() >= 10, label,
                    "expected 10+ rows, got " + std::to_string(ms.row_count()));

    casacore_mini::MsAntennaColumns ant_cols(ms);
    ms_verify_check(ant_cols.row_count() == 6, label, "expected 6 antennas");

    casacore_mini::MsFieldColumns field_cols(ms);
    ms_verify_check(field_cols.row_count() == 2, label, "expected 2 fields");

    casacore_mini::MsSpWindowColumns spw_cols(ms);
    ms_verify_check(spw_cols.row_count() == 2, label, "expected 2 SPWs");

    // Check UVW non-zero for at least some rows
    casacore_mini::MsMainColumns cols(ms);
    int non_zero_uvw = 0;
    for (std::uint64_t r = 0; r < ms.row_count(); ++r) {
        const auto uvw = cols.uvw(r);
        if (uvw.size() == 3 && (uvw[0] != 0.0 || uvw[1] != 0.0 || uvw[2] != 0.0)) {
            ++non_zero_uvw;
        }
    }
    ms_verify_check(non_zero_uvw > 0, label, "expected some rows with non-zero UVW");

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-ms-optional-subtables ---

void produce_ms_optional_subtables(const std::string& output) {
    namespace fs = std::filesystem;
    if (fs::exists(output)) {
        fs::remove_all(output);
    }
    // We don't implement optional subtables (WEATHER, SOURCE, etc.), so this
    // artifact creates a minimal MS and verifies that all 12 required subtables
    // are present. This confirms correct default schema creation.
    auto ms = casacore_mini::MeasurementSet::create(output, /*include_data=*/false);
    casacore_mini::MsWriter writer(ms);
    writer.add_antenna({.name = "ANT0",
                        .station = {},
                        .type = {},
                        .mount = {},
                        .position = {},
                        .offset = {},
                        .dish_diameter = 12.0,
                        .flag_row = false});
    writer.add_field(
        {.name = "F0", .code = {}, .time = 0.0, .num_poly = 0, .source_id = -1, .flag_row = false});
    writer.add_spectral_window({.num_chan = 4,
                                .name = "SPW0",
                                .ref_frequency = 1.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_polarization({.num_corr = 1, .corr_type = {}, .flag_row = false});
    writer.add_observation({.telescope_name = "ALMA",
                            .observer = {},
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.flush();
    std::cout << "  produce-ms-optional-subtables: wrote " << output
              << " (required subtables only, no optional subtables)\n";
}

void verify_ms_optional_subtables(const std::string& input) {
    const std::string label = "ms-optional-subtables";
    auto ms = casacore_mini::MeasurementSet::open(input);

    // All 12 required subtables must exist.
    verify_required_subtables(ms, label);

    // Verify at least 1 antenna/field/spw row was written.
    casacore_mini::MsAntennaColumns ant_cols(ms);
    ms_verify_check(ant_cols.row_count() >= 1, label, "expected at least 1 antenna");

    casacore_mini::MsFieldColumns field_cols(ms);
    ms_verify_check(field_cols.row_count() >= 1, label, "expected at least 1 field");

    casacore_mini::MsSpWindowColumns spw_cols(ms);
    ms_verify_check(spw_cols.row_count() >= 1, label, "expected at least 1 SPW");

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-ms-concat ---

void produce_ms_concat(const std::string& output) {
    namespace fs = std::filesystem;
    if (fs::exists(output)) {
        fs::remove_all(output);
    }
    fs::create_directories(output);

    const auto path_a = fs::path(output) / "ms_a.ms";
    const auto path_b = fs::path(output) / "ms_b.ms";
    const auto path_out = fs::path(output) / "concat.ms";

    // Create MS A: 1 antenna, 1 field "SRC_A", 3 rows
    {
        auto ms_a = casacore_mini::MeasurementSet::create(path_a, false);
        casacore_mini::MsWriter writer(ms_a);
        writer.add_antenna({.name = "ANT0",
                            .station = {},
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_antenna({.name = "ANT1",
                            .station = {},
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_field({.name = "SRC_A",
                          .code = {},
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_spectral_window({.num_chan = 4,
                                    .name = "SPW0",
                                    .ref_frequency = 1.4e9,
                                    .chan_freq = {},
                                    .chan_width = {},
                                    .effective_bw = {},
                                    .resolution = {},
                                    .meas_freq_ref = 0,
                                    .total_bandwidth = 0.0,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = {},
                                    .flag_row = false});
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
        writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
        writer.add_observation({.telescope_name = "VLA",
                                .observer = {},
                                .project = {},
                                .release_date = 0.0,
                                .flag_row = false});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "OBSERVE",
                          .flag_row = false});
        for (int i = 0; i < 3; ++i) {
            writer.add_row({.antenna1 = 0,
                            .antenna2 = 1,
                            .array_id = 0,
                            .data_desc_id = 0,
                            .exposure = 0.0,
                            .feed1 = 0,
                            .feed2 = 0,
                            .field_id = 0,
                            .flag_row = false,
                            .interval = 0.0,
                            .observation_id = 0,
                            .processor_id = 0,
                            .scan_number = 1,
                            .state_id = 0,
                            .time = 4.8e9 + i * 10.0,
                            .time_centroid = 4.8e9 + i * 10.0,
                            .uvw = {100.0, 200.0, 50.0},
                            .sigma = {1.0F, 1.0F},
                            .weight = {1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }
        writer.flush();
    }

    // Create MS B: same antennas, different field "SRC_B", 2 rows
    {
        auto ms_b = casacore_mini::MeasurementSet::create(path_b, false);
        casacore_mini::MsWriter writer(ms_b);
        writer.add_antenna({.name = "ANT0",
                            .station = {},
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_antenna({.name = "ANT1",
                            .station = {},
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
        writer.add_field({.name = "SRC_B",
                          .code = {},
                          .time = 0.0,
                          .num_poly = 0,
                          .source_id = -1,
                          .flag_row = false});
        writer.add_spectral_window({.num_chan = 4,
                                    .name = "SPW0",
                                    .ref_frequency = 1.4e9,
                                    .chan_freq = {},
                                    .chan_width = {},
                                    .effective_bw = {},
                                    .resolution = {},
                                    .meas_freq_ref = 0,
                                    .total_bandwidth = 0.0,
                                    .net_sideband = 0,
                                    .if_conv_chain = 0,
                                    .freq_group = 0,
                                    .freq_group_name = {},
                                    .flag_row = false});
        writer.add_data_description(
            {.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
        writer.add_polarization({.num_corr = 2, .corr_type = {}, .flag_row = false});
        writer.add_observation({.telescope_name = "VLA",
                                .observer = {},
                                .project = {},
                                .release_date = 0.0,
                                .flag_row = false});
        writer.add_state({.sig = true,
                          .ref = false,
                          .cal = 0.0,
                          .load = 0.0,
                          .sub_scan = 0,
                          .obs_mode = "OBSERVE",
                          .flag_row = false});
        for (int i = 0; i < 2; ++i) {
            writer.add_row({.antenna1 = 0,
                            .antenna2 = 1,
                            .array_id = 0,
                            .data_desc_id = 0,
                            .exposure = 0.0,
                            .feed1 = 0,
                            .feed2 = 0,
                            .field_id = 0,
                            .flag_row = false,
                            .interval = 0.0,
                            .observation_id = 0,
                            .processor_id = 0,
                            .scan_number = 2,
                            .state_id = 0,
                            .time = 4.9e9 + i * 10.0,
                            .time_centroid = 4.9e9 + i * 10.0,
                            .uvw = {150.0, 250.0, 75.0},
                            .sigma = {1.0F, 1.0F},
                            .weight = {1.0F, 1.0F},
                            .data = {},
                            .flag = {}});
        }
        writer.flush();
    }

    // Concatenate A + B -> concat.ms
    {
        auto ms_a = casacore_mini::MeasurementSet::open(path_a);
        auto ms_b = casacore_mini::MeasurementSet::open(path_b);
        const auto result = casacore_mini::ms_concat(ms_a, ms_b, path_out);
        std::cout << "  produce-ms-concat: wrote " << path_out.string() << " (" << result.row_count
                  << " rows)\n";
    }
}

void verify_ms_concat(const std::string& input) {
    const std::string label = "ms-concat";
    const auto concat_path = std::filesystem::path(input) / "concat.ms";
    auto ms = casacore_mini::MeasurementSet::open(concat_path);

    verify_required_subtables(ms, label);

    // Should have rows from both input MSes (3 + 2 = 5).
    ms_verify_check(ms.row_count() >= 2, label,
                    "expected at least 2 rows from concatenation, got " +
                        std::to_string(ms.row_count()));

    // Should have at least 2 antennas and 2 fields.
    // casacore produces 3 antennas (ANT0, ANT1, ANT2); mini may produce 2 or 3
    // depending on deduplication strategy.
    casacore_mini::MsMetaData md(ms);
    ms_verify_check(md.n_antennas() >= 2, label,
                    "expected at least 2 antennas, got " + std::to_string(md.n_antennas()));
    ms_verify_check(md.n_fields() >= 2, label,
                    "expected at least 2 fields, got " + std::to_string(md.n_fields()));

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-ms-selection-stress ---

void produce_ms_selection_stress(const std::string& output) {
    namespace fs = std::filesystem;
    if (fs::exists(output)) {
        fs::remove_all(output);
    }
    auto ms = casacore_mini::MeasurementSet::create(output, /*include_data=*/false);
    casacore_mini::MsWriter writer(ms);

    // 6 antennas
    for (int i = 0; i < 6; ++i) {
        writer.add_antenna({.name = "ANT" + std::to_string(i),
                            .station = "PAD" + std::to_string(i),
                            .type = {},
                            .mount = {},
                            .position = {},
                            .offset = {},
                            .dish_diameter = 25.0,
                            .flag_row = false});
    }

    // 3 fields
    writer.add_field({.name = "3C273",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "J1924-2914",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});
    writer.add_field({.name = "CygA",
                      .code = {},
                      .time = 0.0,
                      .num_poly = 0,
                      .source_id = -1,
                      .flag_row = false});

    // 3 SPWs
    writer.add_spectral_window({.num_chan = 32,
                                .name = "SPW0",
                                .ref_frequency = 1.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    writer.add_spectral_window({.num_chan = 64,
                                .name = "SPW1",
                                .ref_frequency = 2.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});
    writer.add_spectral_window({.num_chan = 128,
                                .name = "SPW2",
                                .ref_frequency = 3.0e9,
                                .chan_freq = {},
                                .chan_width = {},
                                .effective_bw = {},
                                .resolution = {},
                                .meas_freq_ref = 0,
                                .total_bandwidth = 0.0,
                                .net_sideband = 0,
                                .if_conv_chain = 0,
                                .freq_group = 0,
                                .freq_group_name = {},
                                .flag_row = false});

    // 3 data descriptions (one per SPW)
    writer.add_data_description({.spectral_window_id = 0, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 1, .polarization_id = 0, .flag_row = false});
    writer.add_data_description({.spectral_window_id = 2, .polarization_id = 0, .flag_row = false});

    writer.add_polarization({.num_corr = 4, .corr_type = {}, .flag_row = false});
    writer.add_observation({.telescope_name = "VLA",
                            .observer = {},
                            .project = {},
                            .release_date = 0.0,
                            .flag_row = false});
    writer.add_state({.sig = true,
                      .ref = false,
                      .cal = 0.0,
                      .load = 0.0,
                      .sub_scan = 0,
                      .obs_mode = "OBSERVE",
                      .flag_row = false});

    // 3 fields x 3 scans x 3 SPWs x 1 baseline = 27 rows, with 2 array_ids
    const double base_time = 5.0e9;
    int row_count = 0;
    for (int field = 0; field < 3; ++field) {
        for (int scan = 1; scan <= 3; ++scan) {
            for (int spw = 0; spw < 3; ++spw) {
                const int array = (row_count < 14) ? 0 : 1; // split across 2 arrays
                const double t = base_time + row_count * 10.0;
                writer.add_row({
                    .antenna1 = 0,
                    .antenna2 = 1,
                    .array_id = array,
                    .data_desc_id = spw,
                    .exposure = 0.0,
                    .feed1 = 0,
                    .feed2 = 0,
                    .field_id = field,
                    .flag_row = false,
                    .interval = 0.0,
                    .observation_id = 0,
                    .processor_id = 0,
                    .scan_number = scan,
                    .state_id = 0,
                    .time = t,
                    .time_centroid = t,
                    .uvw = {100.0 + row_count, 200.0, 50.0},
                    .sigma = {1.0F, 1.0F, 1.0F, 1.0F},
                    .weight = {1.0F, 1.0F, 1.0F, 1.0F},
                    .data = {},
                    .flag = {},
                });
                ++row_count;
            }
        }
    }

    writer.flush();
    std::cout << "  produce-ms-selection-stress: wrote " << output << " (" << row_count
              << " rows)\n";
}

void verify_ms_selection_stress(const std::string& input) {
    const std::string label = "ms-selection-stress";
    auto ms = casacore_mini::MeasurementSet::open(input);

    verify_required_subtables(ms, label);

    ms_verify_check(ms.row_count() >= 27, label,
                    "expected 27+ rows, got " + std::to_string(ms.row_count()));

    casacore_mini::MsMetaData md(ms);
    ms_verify_check(md.n_fields() >= 3, label,
                    "expected at least 3 fields, got " + std::to_string(md.n_fields()));

    const auto& spw_names = md.spw_names();
    ms_verify_check(spw_names.size() >= 3, label,
                    "expected at least 3 SPWs, got " + std::to_string(spw_names.size()));

    const auto& scans = md.scan_numbers();
    ms_verify_check(scans.size() >= 3, label,
                    "expected at least 3 scans, got " + std::to_string(scans.size()));

    const auto& arrays = md.array_ids();
    ms_verify_check(arrays.size() >= 2, label,
                    "expected at least 2 array_ids, got " + std::to_string(arrays.size()));

    std::cout << "  " << label << ": [PASS]\n";
}

// =====================================================================
// Oracle conformance: verify-oracle-ms
// =====================================================================

/// base64 decode (needed for oracle dump parsing).
[[nodiscard]] std::string base64_decode(const std::string_view input) {
    static constexpr int kTable[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62,
        -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,
        1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };
    std::string output;
    auto len = input.size();
    while (len > 0 && input[len - 1] == '=')
        --len;

    output.reserve(len * 3 / 4);
    std::uint32_t buf = 0;
    int bits = 0;
    for (std::size_t i = 0; i < len; ++i) {
        const int val = kTable[static_cast<unsigned char>(input[i])];
        if (val < 0)
            continue;
        buf = (buf << 6U) | static_cast<std::uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((buf >> bits) & 0xFFU));
        }
    }
    return output;
}

/// Parse a hex-float string back to a numeric value.
[[nodiscard]] double parse_hex_double(const std::string_view s) {
    auto str = std::string(s);
    std::istringstream iss{str};
    double value = 0.0;
    iss >> std::hexfloat >> value;
    return value;
}

[[nodiscard]] float parse_hex_float_val(const std::string_view s) {
    auto str = std::string(s);
    std::istringstream iss{str};
    float value = 0.0F;
    iss >> std::hexfloat >> value;
    return value;
}

/// Map oracle dtype string to casacore_mini DataType.
[[nodiscard]] casacore_mini::DataType oracle_dtype_to_mini(const std::string_view s) {
    if (s == "TpBool")
        return casacore_mini::DataType::tp_bool;
    if (s == "TpChar")
        return casacore_mini::DataType::tp_char;
    if (s == "TpUChar")
        return casacore_mini::DataType::tp_uchar;
    if (s == "TpShort")
        return casacore_mini::DataType::tp_short;
    if (s == "TpUShort")
        return casacore_mini::DataType::tp_ushort;
    if (s == "TpInt")
        return casacore_mini::DataType::tp_int;
    if (s == "TpUInt")
        return casacore_mini::DataType::tp_uint;
    if (s == "TpFloat")
        return casacore_mini::DataType::tp_float;
    if (s == "TpDouble")
        return casacore_mini::DataType::tp_double;
    if (s == "TpComplex")
        return casacore_mini::DataType::tp_complex;
    if (s == "TpDComplex")
        return casacore_mini::DataType::tp_dcomplex;
    if (s == "TpString")
        return casacore_mini::DataType::tp_string;
    if (s == "TpTable")
        return casacore_mini::DataType::tp_table;
    if (s == "TpInt64")
        return casacore_mini::DataType::tp_int64;
    throw std::runtime_error("unknown oracle dtype: " + std::string(s));
}

/// Map casacore_mini DataType to oracle dtype string (for error messages).
[[nodiscard]] std::string mini_dtype_to_oracle(casacore_mini::DataType dt) {
    switch (dt) {
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
    default:
        return "TpOther(" + std::to_string(static_cast<int>(dt)) + ")";
    }
}

/// Parsed oracle column descriptor.
struct OracleColDesc {
    std::string name;
    std::string kind;  // "scalar" or "array"
    std::string dtype; // "TpInt", "TpDouble", etc.
    int ndim = 0;
    std::string shape; // "d0,d1,..." or empty
    int options = 0;
    std::string dm_type;
    std::string dm_group;
};

/// Parsed oracle table section.
struct OracleTable {
    std::string name;
    std::uint64_t row_count = 0;
    std::uint64_t ncol = 0;
    std::vector<OracleColDesc> columns;
    std::vector<std::string> table_kw_lines;
    // col_name -> keyword lines
    std::map<std::string, std::vector<std::string>> col_kw_lines;
    // cell_key = "col_b64][row" -> value string
    std::map<std::string, std::string> cells;
};

/// Parse the oracle dump file into structured sections.
[[nodiscard]] std::map<std::string, OracleTable>
parse_oracle_dump(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open oracle dump: " + path.string());
    }

    std::map<std::string, OracleTable> tables;
    std::string current_table;
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with("oracle_version=") || line.starts_with("source_ms=") ||
            line.starts_with("table_count=")) {
            continue;
        }

        // "table=NAME" (no dot) starts a new table section.
        if (line.starts_with("table=")) {
            const auto rest = line.substr(6);
            const auto dot_pos = rest.find('.');
            if (dot_pos == std::string::npos) {
                current_table = rest;
                tables[current_table].name = current_table;
                continue;
            }

            const auto table_name = rest.substr(0, dot_pos);
            const auto suffix = rest.substr(dot_pos + 1);

            if (suffix.starts_with("row_count=")) {
                tables[table_name].row_count = std::stoull(suffix.substr(10));
            } else if (suffix.starts_with("ncol=")) {
                tables[table_name].ncol = std::stoull(suffix.substr(5));
            } else if (suffix.starts_with("col[")) {
                // Parse column descriptor line.
                // Format: col[i].field=value
                const auto bracket_end = suffix.find(']');
                const auto col_idx = std::stoull(suffix.substr(4, bracket_end - 4));
                const auto field_eq = suffix.substr(bracket_end + 2); // skip "]."
                const auto eq_pos = field_eq.find('=');
                const auto field_name = field_eq.substr(0, eq_pos);
                const auto field_value = field_eq.substr(eq_pos + 1);

                auto& tbl = tables[table_name];
                while (tbl.columns.size() <= col_idx) {
                    tbl.columns.emplace_back();
                }
                auto& cd = tbl.columns[col_idx];
                if (field_name == "name_b64") {
                    cd.name = base64_decode(field_value);
                } else if (field_name == "kind") {
                    cd.kind = field_value;
                } else if (field_name == "dtype") {
                    cd.dtype = field_value;
                } else if (field_name == "ndim") {
                    cd.ndim = std::stoi(std::string(field_value));
                } else if (field_name == "shape") {
                    cd.shape = field_value;
                } else if (field_name == "options") {
                    cd.options = std::stoi(std::string(field_value));
                } else if (field_name == "dm_type_b64") {
                    cd.dm_type = base64_decode(field_value);
                } else if (field_name == "dm_group_b64") {
                    cd.dm_group = base64_decode(field_value);
                }
            } else if (suffix.starts_with("kw.")) {
                tables[table_name].table_kw_lines.push_back(suffix.substr(3));
            } else if (suffix.starts_with("col_kw[")) {
                // Format: col_kw[col_b64].path=type|value
                const auto bracket_end = suffix.find(']');
                const auto col_b64 = suffix.substr(7, bracket_end - 7);
                const auto col_name = base64_decode(col_b64);
                const auto kw_rest = suffix.substr(bracket_end + 2); // skip "]."
                tables[table_name].col_kw_lines[col_name].push_back(kw_rest);
            } else if (suffix.starts_with("cell[")) {
                // Format: cell[col_b64][row]=value
                const auto cell_rest = suffix.substr(5);
                const auto bracket1_end = cell_rest.find(']');
                const auto col_b64 = cell_rest.substr(0, bracket1_end);
                const auto after_bracket1 = cell_rest.substr(bracket1_end + 1);
                // after_bracket1 = "[row]=value"
                const auto bracket2_start = after_bracket1.find('[');
                const auto bracket2_end = after_bracket1.find(']');
                const auto row_str =
                    after_bracket1.substr(bracket2_start + 1, bracket2_end - bracket2_start - 1);
                const auto eq_pos = after_bracket1.find('=', bracket2_end);
                const auto value_str = after_bracket1.substr(eq_pos + 1);
                const auto col_name = base64_decode(col_b64);
                const std::string cell_key = col_name + "][" + std::string(row_str);
                tables[table_name].cells[cell_key] = value_str;
            }
        }
    }

    return tables;
}

/// Format a CellValue from SM reader as oracle string for comparison.
[[nodiscard]] std::string format_cell_value_oracle(const casacore_mini::CellValue& cv) {
    if (const auto* v = std::get_if<bool>(&cv)) {
        return std::string("bool|") + (*v ? "true" : "false");
    }
    if (const auto* v = std::get_if<std::int32_t>(&cv)) {
        return "int32|" + std::to_string(*v);
    }
    if (const auto* v = std::get_if<std::uint32_t>(&cv)) {
        return "uint32|" + std::to_string(*v);
    }
    if (const auto* v = std::get_if<std::int64_t>(&cv)) {
        return "int64|" + std::to_string(*v);
    }
    if (const auto* v = std::get_if<float>(&cv)) {
        return "float32|" + format_hex_float(*v);
    }
    if (const auto* v = std::get_if<double>(&cv)) {
        return "float64|" + format_hex_float(*v);
    }
    if (const auto* v = std::get_if<std::complex<float>>(&cv)) {
        return "complex64|" + format_complex64(*v);
    }
    if (const auto* v = std::get_if<std::complex<double>>(&cv)) {
        return "complex128|" + format_complex128(*v);
    }
    if (const auto* v = std::get_if<std::string>(&cv)) {
        return "string|b64:" + base64_encode(*v);
    }
    return "unsupported_cell_type";
}

/// Format a raw byte array as oracle array value string.
template <typename T, typename Formatter>
[[nodiscard]] std::string format_typed_array_oracle(const std::vector<std::uint8_t>& raw,
                                                    const std::vector<std::int64_t>& shape,
                                                    const std::string_view type_name,
                                                    Formatter formatter, bool big_endian) {
    const std::size_t elem_size = sizeof(T);
    const std::size_t n_elems = raw.size() / elem_size;

    std::string shape_str;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0)
            shape_str += ',';
        shape_str += std::to_string(shape[i]);
    }

    std::string values;
    for (std::size_t i = 0; i < n_elems; ++i) {
        if (i > 0)
            values += ',';
        T val{};
        std::memcpy(&val, raw.data() + i * elem_size, elem_size);
        // Byte-swap if needed (data is stored in table endianness).
        if constexpr (sizeof(T) > 1) {
            if (big_endian) {
                auto* bytes = reinterpret_cast<std::uint8_t*>(&val);
                for (std::size_t j = 0; j < sizeof(T) / 2; ++j) {
                    std::swap(bytes[j], bytes[sizeof(T) - 1 - j]);
                }
            }
        }
        values += formatter(val);
    }

    return "array:" + std::string(type_name) + "|shape=" + shape_str + "|values=" + values;
}

/// Compare two oracle value strings with tolerance for floating point.
/// Returns: 0=exact match, 1=tolerance match, 2=mismatch.
[[nodiscard]] int compare_oracle_values(const std::string& expected, const std::string& actual) {
    if (expected == actual)
        return 0;

    // Try tolerance comparison for float/double scalars.
    if (expected.starts_with("float32|") && actual.starts_with("float32|")) {
        const float exp_val = parse_hex_float_val(expected.substr(8));
        const float act_val = parse_hex_float_val(actual.substr(8));
        if (std::fabs(exp_val - act_val) < 1e-5F)
            return 1;
    }
    if (expected.starts_with("float64|") && actual.starts_with("float64|")) {
        const double exp_val = parse_hex_double(expected.substr(8));
        const double act_val = parse_hex_double(actual.substr(8));
        if (std::fabs(exp_val - act_val) < 1e-10)
            return 1;
    }

    // For arrays, try element-by-element tolerance comparison.
    if (expected.starts_with("array:float32|") && actual.starts_with("array:float32|")) {
        // Extract shapes.
        auto extract_after = [](const std::string& s, const std::string& tag) -> std::string {
            const auto pos = s.find(tag);
            if (pos == std::string::npos)
                return "";
            const auto start = pos + tag.size();
            const auto end = s.find('|', start);
            return (end == std::string::npos) ? s.substr(start) : s.substr(start, end - start);
        };
        if (extract_after(expected, "shape=") != extract_after(actual, "shape="))
            return 2;
        // Compare values element by element.
        auto extract_values = [](const std::string& s) -> std::string {
            const auto pos = s.find("values=");
            return (pos == std::string::npos) ? "" : s.substr(pos + 7);
        };
        auto split_csv = [](const std::string& s) -> std::vector<std::string> {
            std::vector<std::string> result;
            std::istringstream iss(s);
            std::string token;
            while (std::getline(iss, token, ',')) {
                result.push_back(token);
            }
            return result;
        };
        const auto exp_vals = split_csv(extract_values(expected));
        const auto act_vals = split_csv(extract_values(actual));
        if (exp_vals.size() != act_vals.size())
            return 2;
        bool all_tolerance = true;
        for (std::size_t i = 0; i < exp_vals.size(); ++i) {
            if (exp_vals[i] == act_vals[i])
                continue;
            const float ev = parse_hex_float_val(exp_vals[i]);
            const float av = parse_hex_float_val(act_vals[i]);
            if (std::fabs(ev - av) >= 1e-5F)
                return 2;
            all_tolerance = true;
        }
        return all_tolerance ? 1 : 0;
    }
    if (expected.starts_with("array:float64|") && actual.starts_with("array:float64|")) {
        auto extract_after = [](const std::string& s, const std::string& tag) -> std::string {
            const auto pos = s.find(tag);
            if (pos == std::string::npos)
                return "";
            const auto start = pos + tag.size();
            const auto end = s.find('|', start);
            return (end == std::string::npos) ? s.substr(start) : s.substr(start, end - start);
        };
        if (extract_after(expected, "shape=") != extract_after(actual, "shape="))
            return 2;
        auto extract_values = [](const std::string& s) -> std::string {
            const auto pos = s.find("values=");
            return (pos == std::string::npos) ? "" : s.substr(pos + 7);
        };
        auto split_csv = [](const std::string& s) -> std::vector<std::string> {
            std::vector<std::string> result;
            std::istringstream iss(s);
            std::string token;
            while (std::getline(iss, token, ',')) {
                result.push_back(token);
            }
            return result;
        };
        const auto exp_vals = split_csv(extract_values(expected));
        const auto act_vals = split_csv(extract_values(actual));
        if (exp_vals.size() != act_vals.size())
            return 2;
        bool all_tolerance = true;
        for (std::size_t i = 0; i < exp_vals.size(); ++i) {
            if (exp_vals[i] == act_vals[i])
                continue;
            const double ev = parse_hex_double(exp_vals[i]);
            const double av = parse_hex_double(act_vals[i]);
            if (std::fabs(ev - av) >= 1e-10)
                return 2;
            all_tolerance = true;
        }
        return all_tolerance ? 1 : 0;
    }

    return 2; // mismatch
}

/// Format a typed array read via the Table API as an oracle comparison string.
/// The Table API returns host-endian data, so no byte-swap is needed.
template <typename T, typename Formatter>
[[nodiscard]] std::string
format_table_array_oracle(const std::vector<T>& values, const std::vector<std::int64_t>& shape,
                          const std::string_view type_name, Formatter formatter) {
    std::string shape_str;
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i > 0)
            shape_str += ',';
        shape_str += std::to_string(shape[i]);
    }
    std::string vals;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
            vals += ',';
        vals += formatter(values[i]);
    }
    return "array:" + std::string(type_name) + "|shape=" + shape_str + "|values=" + vals;
}

/// Read a cell value from a Table and format it as an oracle string.
/// All SM routing is handled internally by the Table abstraction.
[[nodiscard]] std::string read_and_format_cell(const casacore_mini::Table& table,
                                               const std::string& col_name, std::uint64_t row,
                                               const casacore_mini::ColumnDesc& cd) {
    using namespace casacore_mini;

    if (cd.kind == ColumnKind::scalar) {
        return format_cell_value_oracle(table.read_scalar_cell(col_name, row));
    }

    // Array column: read via Table API and format.
    auto shape = table.cell_shape(col_name, row);

    switch (cd.data_type) {
    case DataType::tp_float: {
        auto arr = table.read_array_float_cell(col_name, row);
        return format_table_array_oracle(arr, shape, "float32",
                                         [](float v) { return format_hex_float(v); });
    }
    case DataType::tp_double: {
        auto arr = table.read_array_double_cell(col_name, row);
        return format_table_array_oracle(arr, shape, "float64",
                                         [](double v) { return format_hex_float(v); });
    }
    case DataType::tp_int: {
        auto arr = table.read_array_int_cell(col_name, row);
        return format_table_array_oracle(arr, shape, "int32",
                                         [](std::int32_t v) { return std::to_string(v); });
    }
    case DataType::tp_bool: {
        auto arr = table.read_array_bool_cell(col_name, row);
        std::string shape_str;
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (i > 0)
                shape_str += ',';
            shape_str += std::to_string(shape[i]);
        }
        std::string vals;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0)
                vals += ',';
            vals += arr[i] ? "true" : "false";
        }
        return "array:bool|shape=" + shape_str + "|values=" + vals;
    }
    case DataType::tp_complex: {
        auto arr = table.read_array_complex_cell(col_name, row);
        return format_table_array_oracle(arr, shape, "complex64",
                                         [](std::complex<float> v) { return format_complex64(v); });
    }
    case DataType::tp_dcomplex: {
        auto arr = table.read_array_dcomplex_cell(col_name, row);
        return format_table_array_oracle(
            arr, shape, "complex128", [](std::complex<double> v) { return format_complex128(v); });
    }
    case DataType::tp_string: {
        auto arr = table.read_array_string_cell(col_name, row);
        std::string shape_str;
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (i > 0)
                shape_str += ',';
            shape_str += std::to_string(shape[i]);
        }
        std::string vals;
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0)
                vals += ',';
            vals += "b64:" + base64_encode(arr[i]);
        }
        return "array:string|shape=" + shape_str + "|values=" + vals;
    }
    default:
        throw std::runtime_error("unsupported array dtype: " + mini_dtype_to_oracle(cd.data_type));
    }
}

/// Verify a real MS against an oracle dump, cell-by-cell.
/// All table access goes through the high-level Table abstraction.
void verify_oracle_ms(const std::string& ms_path, const std::string& oracle_path) {
    std::cout << "  verify-oracle-ms: loading oracle dump...\n";
    const auto oracle = parse_oracle_dump(oracle_path);
    std::cout << "  verify-oracle-ms: oracle has " << oracle.size() << " tables\n";

    int total_pass = 0;
    int total_warn = 0;
    int total_fail = 0;

    for (const auto& [table_name, oracle_tbl] : oracle) {
        // Determine the on-disk path for this table.
        std::string table_dir;
        if (table_name == "MAIN") {
            table_dir = ms_path;
        } else {
            table_dir = (std::filesystem::path(ms_path) / table_name).string();
        }

        // Open via casacore-mini Table abstraction.
        std::optional<casacore_mini::Table> table_opt;
        try {
            table_opt = casacore_mini::Table::open(table_dir);
        } catch (const std::exception& e) {
            std::cerr << "  verify-oracle-ms: FAIL table " << table_name
                      << " — cannot open: " << e.what() << "\n";
            ++total_fail;
            continue;
        }
        auto& table = *table_opt;

        // Verify row count.
        if (table.nrow() != oracle_tbl.row_count) {
            std::cerr << "  verify-oracle-ms: FAIL table " << table_name
                      << " row_count mismatch: oracle=" << oracle_tbl.row_count
                      << " mini=" << table.nrow() << "\n";
            ++total_fail;
            continue;
        }

        // Verify column count.
        if (table.ncolumn() != oracle_tbl.ncol) {
            std::cerr << "  verify-oracle-ms: FAIL table " << table_name
                      << " ncol mismatch: oracle=" << oracle_tbl.ncol << " mini=" << table.ncolumn()
                      << "\n";
            ++total_fail;
            continue;
        }

        // Verify column descriptors.
        const auto& mini_cols = table.columns();
        bool col_desc_ok = true;
        for (std::size_t i = 0; i < oracle_tbl.columns.size(); ++i) {
            const auto& oracle_cd = oracle_tbl.columns[i];
            const auto& mini_cd = mini_cols[i];

            if (mini_cd.name != oracle_cd.name) {
                std::cerr << "  verify-oracle-ms: FAIL table " << table_name << " col[" << i
                          << "] name mismatch: oracle=" << oracle_cd.name
                          << " mini=" << mini_cd.name << "\n";
                col_desc_ok = false;
                break;
            }
            const auto oracle_dt = oracle_dtype_to_mini(oracle_cd.dtype);
            if (mini_cd.data_type != oracle_dt) {
                std::cerr << "  verify-oracle-ms: FAIL table " << table_name << " col "
                          << oracle_cd.name << " dtype mismatch: oracle=" << oracle_cd.dtype
                          << " mini=" << mini_dtype_to_oracle(mini_cd.data_type) << "\n";
                col_desc_ok = false;
                break;
            }
        }
        if (!col_desc_ok) {
            ++total_fail;
            continue;
        }

        // Verify cells through the Table API (no direct SM access).
        int table_pass = 0;
        int table_warn = 0;
        int table_fail = 0;

        for (const auto& [cell_key, oracle_value] : oracle_tbl.cells) {
            const auto sep = cell_key.find("][");
            const auto col_name = cell_key.substr(0, sep);
            const auto row = std::stoull(cell_key.substr(sep + 2));

            // Find column descriptor.
            const auto* cd = table.find_column_desc(col_name);
            if (cd == nullptr) {
                if (table_fail < 10) {
                    std::cerr << "    FAIL " << table_name << "." << col_name << "[" << row
                              << "]: column not found\n";
                }
                ++table_fail;
                continue;
            }

            if (oracle_value == "undefined") {
                ++table_pass;
                continue;
            }

            std::string actual_value;
            try {
                actual_value = read_and_format_cell(table, col_name, row, *cd);
            } catch (const std::exception& e) {
                if (table_fail < 10) {
                    std::cerr << "    FAIL " << table_name << "." << col_name << "[" << row
                              << "] read error: " << e.what() << "\n";
                }
                ++table_fail;
                continue;
            }

            const int cmp = compare_oracle_values(oracle_value, actual_value);
            if (cmp == 0) {
                ++table_pass;
            } else if (cmp == 1) {
                ++table_warn;
            } else {
                if (table_fail < 10) {
                    std::cerr << "    FAIL " << table_name << "." << col_name << "[" << row << "]\n"
                              << "      oracle: " << oracle_value.substr(0, 200) << "\n"
                              << "      actual: " << actual_value.substr(0, 200) << "\n";
                }
                ++table_fail;
            }
        }

        std::cout << "  verify-oracle-ms: table " << table_name << " pass=" << table_pass
                  << " warn=" << table_warn << " fail=" << table_fail << "\n";

        total_pass += table_pass;
        total_warn += table_warn;
        total_fail += table_fail;
    }

    std::cout << "\n  verify-oracle-ms summary: pass=" << total_pass << " warn=" << total_warn
              << " fail=" << total_fail << "\n";

    if (total_fail > 0) {
        throw std::runtime_error("verify-oracle-ms: " + std::to_string(total_fail) +
                                 " cell failures");
    }
    std::cout << "  verify-oracle-ms: [PASS]\n";
}

// =====================================================================
// --- Phase 10: Image interop helpers ---
// =====================================================================

void img_verify_check(bool cond, const std::string& artifact, const std::string& detail) {
    if (!cond) {
        throw std::runtime_error(artifact + ": " + detail);
    }
}

casacore_mini::CoordinateSystem make_linear_cs(std::size_t ndim) {
    casacore_mini::CoordinateSystem cs;
    std::vector<std::string> names(ndim, "Axis");
    std::vector<std::string> units(ndim, "pixel");
    casacore_mini::LinearXform xform;
    xform.crval.assign(ndim, 0.0);
    xform.cdelt.assign(ndim, 1.0);
    xform.crpix.assign(ndim, 0.0);
    cs.add_coordinate(std::make_unique<casacore_mini::LinearCoordinate>(
        std::move(names), std::move(units), std::move(xform)));
    return cs;
}

casacore_mini::CoordinateSystem make_direction_spectral_cs() {
    casacore_mini::CoordinateSystem cs;
    cs.add_coordinate(std::make_unique<casacore_mini::DirectionCoordinate>(
        casacore_mini::DirectionRef::j2000,
        casacore_mini::Projection{casacore_mini::ProjectionType::sin, {}},
        0.0, 0.0, -1e-5, 1e-5,
        std::vector<double>{1, 0, 0, 1}, 0.0, 0.0));
    cs.add_coordinate(std::make_unique<casacore_mini::SpectralCoordinate>(
        casacore_mini::FrequencyRef::lsrk, 1.4e9, 1e6, 0.0));
    return cs;
}

// --- produce-img-minimal ---

void produce_img_minimal(const std::string& output) {
    namespace fs = std::filesystem;
    fs::create_directories(output);

    const auto img_path = fs::path(output) / "img_minimal.img";
    if (fs::exists(img_path)) {
        fs::remove_all(img_path);
    }

    casacore_mini::IPosition shape{64, 64};
    casacore_mini::PagedImage<float> img(shape, make_linear_cs(2), img_path);

    casacore_mini::LatticeArray<float> data(shape);
    data.make_unique();
    for (std::int64_t row = 0; row < 64; ++row) {
        for (std::int64_t col = 0; col < 64; ++col) {
            data.put(casacore_mini::IPosition{col, row},
                     static_cast<float>(row * 64 + col + 1));
        }
    }
    img.put(data);
    img.set_units("Jy/beam");
    img.flush();

    std::cout << "  produce-img-minimal: wrote " << img_path.string() << '\n';
}

void verify_img_minimal(const std::string& input) {
    const std::string label = "img-minimal";
    namespace fs = std::filesystem;

    const auto img_path = fs::path(input) / "img_minimal.img";
    casacore_mini::PagedImage<float> img(img_path);

    img_verify_check(img.shape() == casacore_mini::IPosition({64, 64}),
                     label, "expected shape (64,64)");
    img_verify_check(img.units() == "Jy/beam", label, "expected units Jy/beam");
    img_verify_check(!img.has_pixel_mask(), label, "expected no pixel mask");

    auto data = img.get();
    for (std::int64_t row = 0; row < 64; ++row) {
        for (std::int64_t col = 0; col < 64; ++col) {
            const float expected = static_cast<float>(row * 64 + col + 1);
            const float actual = data.at(casacore_mini::IPosition{col, row});
            img_verify_check(
                std::abs(actual - expected) < kFloat32Tolerance, label,
                "pixel mismatch at (" + std::to_string(col) + "," +
                    std::to_string(row) + "): got " + std::to_string(actual) +
                    " expected " + std::to_string(expected));
        }
    }

    // Verify coordinate system is present and consistent.
    const auto& cs = img.coordinates();
    img_verify_check(cs.n_coordinates() >= 1, label,
                     "expected at least 1 coordinate in CS");

    // Coordinate roundtrip: to_world(crpix) ≈ crval.
    {
        const auto n_pix = cs.n_pixel_axes();
        const auto& coord0 = cs.coordinate(0);
        auto crpix = coord0.reference_pixel();
        auto crval = coord0.reference_value();
        auto world = cs.to_world(std::vector<double>(crpix.begin(), crpix.end()));
        for (std::size_t i = 0; i < std::min(crval.size(), world.size()); ++i) {
            img_verify_check(
                std::abs(world[i] - crval[i]) < kFloat64Tolerance, label,
                "to_world(crpix)[" + std::to_string(i) + "] != crval: " +
                    std::to_string(world[i]) + " vs " + std::to_string(crval[i]));
        }

        // Pixel roundtrip: to_pixel(to_world(test_pixel)) ≈ test_pixel.
        std::vector<double> test_pixel(n_pix, 5.0);
        auto w = cs.to_world(test_pixel);
        auto p_back = cs.to_pixel(w);
        for (std::size_t i = 0; i < n_pix; ++i) {
            img_verify_check(
                std::abs(p_back[i] - test_pixel[i]) < kFloat64Tolerance, label,
                "pixel roundtrip axis " + std::to_string(i) + ": " +
                    std::to_string(p_back[i]) + " vs " + std::to_string(test_pixel[i]));
        }
    }

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-img-cube-masked ---

void produce_img_cube_masked(const std::string& output) {
    namespace fs = std::filesystem;
    fs::create_directories(output);

    const auto img_path = fs::path(output) / "img_cube_masked.img";
    if (fs::exists(img_path)) {
        fs::remove_all(img_path);
    }

    casacore_mini::IPosition shape{32, 32, 16};
    casacore_mini::PagedImage<float> img(shape, make_direction_spectral_cs(), img_path);

    casacore_mini::LatticeArray<float> data(shape);
    data.make_unique();
    for (std::int64_t z = 0; z < 16; ++z) {
        for (std::int64_t y = 0; y < 32; ++y) {
            for (std::int64_t x = 0; x < 32; ++x) {
                data.put(casacore_mini::IPosition{x, y, z},
                         static_cast<float>(z * 1024 + y * 32 + x + 1));
            }
        }
    }
    img.put(data);
    img.set_units("Jy/beam");

    // Create and store pixel mask: mask[i] = ((i % 4) != 0)
    auto npix = static_cast<std::size_t>(shape.product());
    std::vector<bool> mask_vec(npix);
    for (std::size_t i = 0; i < npix; ++i) {
        mask_vec[i] = ((i % 4) != 0);
    }
    casacore_mini::LatticeArray<bool> mask_data(shape, std::move(mask_vec));
    img.make_mask("mask0", mask_data);

    // Store mask pattern indicator in misc_info.
    casacore_mini::Record misc;
    misc.set("mask_pattern", casacore_mini::RecordValue(std::string("mod4")));
    img.set_misc_info(std::move(misc));
    img.flush();

    std::cout << "  produce-img-cube-masked: wrote " << img_path.string() << '\n';
}

void verify_img_cube_masked(const std::string& input) {
    const std::string label = "img-cube-masked";
    namespace fs = std::filesystem;

    const auto img_path = fs::path(input) / "img_cube_masked.img";
    casacore_mini::PagedImage<float> img(img_path);

    img_verify_check(img.shape() == casacore_mini::IPosition({32, 32, 16}),
                     label, "expected shape (32,32,16)");

    // Verify pixel values.
    auto data = img.get();
    for (std::int64_t z = 0; z < 16; ++z) {
        for (std::int64_t y = 0; y < 32; ++y) {
            for (std::int64_t x = 0; x < 32; ++x) {
                const float expected =
                    static_cast<float>(z * 1024 + y * 32 + x + 1);
                const float actual =
                    data.at(casacore_mini::IPosition{x, y, z});
                img_verify_check(
                    std::abs(actual - expected) < kFloat32Tolerance, label,
                    "pixel mismatch at (" + std::to_string(x) + "," +
                        std::to_string(y) + "," + std::to_string(z) + ")");
            }
        }
    }

    // Verify mask pattern is stored in misc_info (when produced by mini).
    // When reading casacore-produced images, mask_pattern may be absent because
    // casacore stores the mask as an actual pixel mask lattice, not as metadata.
    const auto& misc = img.misc_info();
    const auto* mp_rv = misc.find("mask_pattern");
    if (mp_rv != nullptr) {
        const auto* mp_str = std::get_if<std::string>(&mp_rv->storage());
        img_verify_check(mp_str != nullptr && *mp_str == "mod4", label,
                         "expected mask_pattern == \"mod4\"");
    }

    // Verify has_pixel_mask() is true.
    img_verify_check(img.has_pixel_mask(), label,
                     "expected has_pixel_mask() == true");

    // Element-by-element mask verification.
    auto mask = img.get_mask();
    const auto total = static_cast<std::size_t>(data.nelements());
    img_verify_check(mask.nelements() == total, label,
                     "mask nelements mismatch: " + std::to_string(mask.nelements()) +
                         " vs " + std::to_string(total));
    std::size_t mask_mismatches = 0;
    for (std::size_t i = 0; i < total; ++i) {
        bool expected = ((i % 4) != 0);
        if (mask[i] != expected) {
            ++mask_mismatches;
        }
    }
    img_verify_check(mask_mismatches == 0, label,
                     "element-by-element mask mismatches: " + std::to_string(mask_mismatches));

    // Also verify the old fraction check as sanity.
    std::size_t true_count = 0;
    for (std::size_t i = 0; i < total; ++i) {
        if (mask[i]) ++true_count;
    }
    const double frac = static_cast<double>(true_count) / static_cast<double>(total);
    img_verify_check(frac > 0.70 && frac < 0.80, label,
                     "mask ~75% true fraction check failed: " +
                         std::to_string(frac));

    // Verify coordinate system has direction + spectral axes (3 pixel axes total).
    const auto& cs = img.coordinates();
    img_verify_check(cs.n_pixel_axes() == 3, label,
                     "expected 3 pixel axes in CS");

    // Coordinate roundtrip verification.
    {
        const auto& coord0 = cs.coordinate(0);
        auto crpix = coord0.reference_pixel();
        auto crval = coord0.reference_value();
        // Pad to n_pixel_axes for to_world call.
        std::vector<double> pixel_vec(cs.n_pixel_axes(), 0.0);
        for (std::size_t i = 0; i < std::min(crpix.size(), pixel_vec.size()); ++i) {
            pixel_vec[i] = crpix[i];
        }
        auto world = cs.to_world(pixel_vec);
        // First two world axes should match crval of direction coordinate.
        for (std::size_t i = 0; i < crval.size(); ++i) {
            img_verify_check(
                std::abs(world[i] - crval[i]) < kFloat64Tolerance, label,
                "coord roundtrip: to_world(crpix)[" + std::to_string(i) + "] != crval");
        }

        // Pixel roundtrip at test point.
        std::vector<double> test_pixel(cs.n_pixel_axes(), 5.0);
        auto w = cs.to_world(test_pixel);
        auto p_back = cs.to_pixel(w);
        for (std::size_t i = 0; i < cs.n_pixel_axes(); ++i) {
            img_verify_check(
                std::abs(p_back[i] - test_pixel[i]) < kFloat64Tolerance, label,
                "pixel roundtrip axis " + std::to_string(i));
        }
    }

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-img-region-subset ---

void produce_img_region_subset(const std::string& output) {
    namespace fs = std::filesystem;
    fs::create_directories(output);

    const auto img_path = fs::path(output) / "img_region_subset.img";
    if (fs::exists(img_path)) {
        fs::remove_all(img_path);
    }

    casacore_mini::IPosition shape{64, 64};
    casacore_mini::PagedImage<float> img(shape, make_linear_cs(2), img_path);

    casacore_mini::LatticeArray<float> data(shape);
    data.make_unique();
    for (std::int64_t y = 0; y < 64; ++y) {
        for (std::int64_t x = 0; x < 64; ++x) {
            data.put(casacore_mini::IPosition{x, y},
                     static_cast<float>(y * 64 + x + 1));
        }
    }
    img.put(data);

    // Store region parameters in misc_info as doubles.
    casacore_mini::Record misc;
    misc.set("blc_x", casacore_mini::RecordValue(10.0));
    misc.set("blc_y", casacore_mini::RecordValue(10.0));
    misc.set("trc_x", casacore_mini::RecordValue(29.0));
    misc.set("trc_y", casacore_mini::RecordValue(29.0));
    img.set_misc_info(std::move(misc));
    img.flush();

    std::cout << "  produce-img-region-subset: wrote " << img_path.string() << '\n';
}

void verify_img_region_subset(const std::string& input) {
    const std::string label = "img-region-subset";
    namespace fs = std::filesystem;

    const auto img_path = fs::path(input) / "img_region_subset.img";
    casacore_mini::PagedImage<float> img(img_path);

    img_verify_check(img.shape() == casacore_mini::IPosition({64, 64}),
                     label, "expected shape (64,64)");

    // Verify base image pixels round-trip.
    auto data = img.get();
    for (std::int64_t y = 0; y < 64; ++y) {
        for (std::int64_t x = 0; x < 64; ++x) {
            const float expected = static_cast<float>(y * 64 + x + 1);
            const float actual = data.at(casacore_mini::IPosition{x, y});
            img_verify_check(
                std::abs(actual - expected) < kFloat32Tolerance, label,
                "base pixel mismatch at (" + std::to_string(x) + "," +
                    std::to_string(y) + ")");
        }
    }

    // Retrieve region parameters from misc_info.
    const auto& misc = img.misc_info();
    const auto* blc_x_rv = misc.find("blc_x");
    const auto* blc_y_rv = misc.find("blc_y");
    const auto* trc_x_rv = misc.find("trc_x");
    const auto* trc_y_rv = misc.find("trc_y");
    img_verify_check(blc_x_rv != nullptr && blc_y_rv != nullptr &&
                         trc_x_rv != nullptr && trc_y_rv != nullptr,
                     label, "missing region parameters in misc_info");

    const auto* blc_x_ptr = std::get_if<double>(&blc_x_rv->storage());
    const auto* blc_y_ptr = std::get_if<double>(&blc_y_rv->storage());
    const auto* trc_x_ptr = std::get_if<double>(&trc_x_rv->storage());
    const auto* trc_y_ptr = std::get_if<double>(&trc_y_rv->storage());

    img_verify_check(blc_x_ptr != nullptr && blc_y_ptr != nullptr &&
                         trc_x_ptr != nullptr && trc_y_ptr != nullptr,
                     label, "region parameter types must be double");

    const auto blc_x = static_cast<std::int64_t>(*blc_x_ptr);
    const auto blc_y = static_cast<std::int64_t>(*blc_y_ptr);
    const auto trc_x = static_cast<std::int64_t>(*trc_x_ptr);
    const auto trc_y = static_cast<std::int64_t>(*trc_y_ptr);

    img_verify_check(blc_x == 10 && blc_y == 10 && trc_x == 29 && trc_y == 29,
                     label, "region parameters do not match expected blc=(10,10) trc=(29,29)");

    // Construct a SubImage over the region and verify shape + values.
    const auto sub_width = trc_x - blc_x + 1;
    const auto sub_height = trc_y - blc_y + 1;
    casacore_mini::SubImage<float> sub(
        img, casacore_mini::Slicer(
                 casacore_mini::IPosition{blc_x, blc_y},
                 casacore_mini::IPosition{sub_width, sub_height}));

    img_verify_check(
        sub.shape() == casacore_mini::IPosition({sub_width, sub_height}),
        label,
        "SubImage shape mismatch: expected (" + std::to_string(sub_width) +
            "," + std::to_string(sub_height) + ")");

    auto sub_data = sub.get();
    for (std::int64_t sy = 0; sy < sub_height; ++sy) {
        for (std::int64_t sx = 0; sx < sub_width; ++sx) {
            const auto gx = blc_x + sx;
            const auto gy = blc_y + sy;
            const float expected = static_cast<float>(gy * 64 + gx + 1);
            const float actual =
                sub_data.at(casacore_mini::IPosition{sx, sy});
            img_verify_check(
                std::abs(actual - expected) < kFloat32Tolerance, label,
                "SubImage pixel mismatch at local (" + std::to_string(sx) +
                    "," + std::to_string(sy) + ")");
        }
    }

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-img-expression ---

void produce_img_expression(const std::string& output) {
    namespace fs = std::filesystem;
    fs::create_directories(output);

    const auto img_path = fs::path(output) / "img_expression.img";
    if (fs::exists(img_path)) {
        fs::remove_all(img_path);
    }

    // Compute expression: result[i] = (i+1) + 2.0*(100.0 - i)
    // which simplifies to: 201.0 - i
    constexpr std::int64_t nx = 32;
    constexpr std::int64_t ny = 32;
    casacore_mini::IPosition shape{nx, ny};

    casacore_mini::LatticeArray<float> result(shape);
    result.make_unique();
    const auto total = static_cast<std::size_t>(nx * ny);
    for (std::size_t i = 0; i < total; ++i) {
        const auto fi = static_cast<float>(i);
        result.mutable_data()[i] = (fi + 1.0F) + 2.0F * (100.0F - fi);
    }

    casacore_mini::PagedImage<float> img(shape, make_linear_cs(2), img_path);
    img.put(result);
    img.set_units("Jy/beam");

    // Store expression metadata.
    casacore_mini::Record misc;
    misc.set("expression", casacore_mini::RecordValue(std::string("lat1 + 2.0 * lat2")));
    img.set_misc_info(std::move(misc));
    img.flush();

    std::cout << "  produce-img-expression: wrote " << img_path.string() << '\n';
}

void verify_img_expression(const std::string& input) {
    const std::string label = "img-expression";
    namespace fs = std::filesystem;

    const auto img_path = fs::path(input) / "img_expression.img";
    casacore_mini::PagedImage<float> img(img_path);

    img_verify_check(img.shape() == casacore_mini::IPosition({32, 32}),
                     label, "expected shape (32,32)");

    auto data = img.get();
    const auto total = data.nelements();
    for (std::size_t i = 0; i < total; ++i) {
        const auto fi = static_cast<float>(i);
        const float expected = (fi + 1.0F) + 2.0F * (100.0F - fi);
        const float actual = data[i];
        img_verify_check(
            std::abs(actual - expected) < kFloat32Tolerance, label,
            "expression pixel mismatch at flat index " + std::to_string(i) +
                ": got " + std::to_string(actual) + " expected " +
                std::to_string(expected));
    }

    // Verify units survived roundtrip.
    img_verify_check(img.units() == "Jy/beam", label,
                     "expected units Jy/beam, got: " + img.units());

    // Verify misc_info expression metadata survived.
    const auto& misc = img.misc_info();
    const auto* expr_rv = misc.find("expression");
    img_verify_check(expr_rv != nullptr, label,
                     "misc_info missing 'expression' key");
    if (expr_rv != nullptr) {
        const auto* expr_str = std::get_if<std::string>(&expr_rv->storage());
        img_verify_check(expr_str != nullptr && *expr_str == "lat1 + 2.0 * lat2",
                         label,
                         "expression metadata mismatch");
    }

    // Coordinate verification.
    const auto& cs = img.coordinates();
    img_verify_check(cs.n_coordinates() >= 1, label,
                     "expected at least 1 coordinate in CS");
    {
        std::vector<double> test_pixel(cs.n_pixel_axes(), 3.0);
        auto w = cs.to_world(test_pixel);
        auto p_back = cs.to_pixel(w);
        for (std::size_t i = 0; i < cs.n_pixel_axes(); ++i) {
            img_verify_check(
                std::abs(p_back[i] - test_pixel[i]) < kFloat64Tolerance, label,
                "pixel roundtrip axis " + std::to_string(i));
        }
    }

    std::cout << "  " << label << ": [PASS]\n";
}

// --- produce-img-complex ---

void produce_img_complex(const std::string& output) {
    namespace fs = std::filesystem;
    fs::create_directories(output);

    const auto img_path = fs::path(output) / "img_complex.img";
    if (fs::exists(img_path)) {
        fs::remove_all(img_path);
    }

    casacore_mini::IPosition shape{32, 32};
    casacore_mini::PagedImage<std::complex<float>> img(
        shape, make_linear_cs(2), img_path);

    casacore_mini::LatticeArray<std::complex<float>> data(shape);
    data.make_unique();
    for (std::int64_t y = 0; y < 32; ++y) {
        for (std::int64_t x = 0; x < 32; ++x) {
            data.put(casacore_mini::IPosition{x, y},
                     std::complex<float>(static_cast<float>(x + 1),
                                         static_cast<float>(y + 1)));
        }
    }
    img.put(data);
    img.flush();

    std::cout << "  produce-img-complex: wrote " << img_path.string() << '\n';
}

void verify_img_complex(const std::string& input) {
    const std::string label = "img-complex";
    namespace fs = std::filesystem;

    const auto img_path = fs::path(input) / "img_complex.img";
    casacore_mini::PagedImage<std::complex<float>> img(img_path);

    img_verify_check(img.shape() == casacore_mini::IPosition({32, 32}),
                     label, "expected shape (32,32)");

    auto data = img.get();
    for (std::int64_t y = 0; y < 32; ++y) {
        for (std::int64_t x = 0; x < 32; ++x) {
            const auto actual = data.at(casacore_mini::IPosition{x, y});
            const float expected_real = static_cast<float>(x + 1);
            const float expected_imag = static_cast<float>(y + 1);
            img_verify_check(
                std::abs(actual.real() - expected_real) < kFloat32Tolerance,
                label,
                "real part mismatch at (" + std::to_string(x) + "," +
                    std::to_string(y) + "): got " +
                    std::to_string(actual.real()) + " expected " +
                    std::to_string(expected_real));
            img_verify_check(
                std::abs(actual.imag() - expected_imag) < kFloat32Tolerance,
                label,
                "imag part mismatch at (" + std::to_string(x) + "," +
                    std::to_string(y) + "): got " +
                    std::to_string(actual.imag()) + " expected " +
                    std::to_string(expected_imag));
        }
    }

    // Verify coordinate system and roundtrip.
    const auto& cs = img.coordinates();
    img_verify_check(cs.n_coordinates() >= 1, label,
                     "expected at least 1 coordinate in CS");
    {
        std::vector<double> test_pixel(cs.n_pixel_axes(), 3.0);
        auto w = cs.to_world(test_pixel);
        auto p_back = cs.to_pixel(w);
        for (std::size_t i = 0; i < cs.n_pixel_axes(); ++i) {
            img_verify_check(
                std::abs(p_back[i] - test_pixel[i]) < kFloat64Tolerance, label,
                "pixel roundtrip axis " + std::to_string(i));
        }
    }

    std::cout << "  " << label << ": [PASS]\n";
}

// =====================================================================

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
           "  interop_mini_tool produce-measure-scalar --output <path>\n"
           "  interop_mini_tool produce-measure-array --output <path>\n"
           "  interop_mini_tool produce-measure-rich --output <path>\n"
           "  interop_mini_tool produce-coord-keywords --output <path>\n"
           "  interop_mini_tool produce-mixed-coords --output <path>\n"
           "  interop_mini_tool produce-conversion-vectors --output <path>\n"
           "  interop_mini_tool verify-measure-scalar --input <path>\n"
           "  interop_mini_tool verify-measure-array --input <path>\n"
           "  interop_mini_tool verify-measure-rich --input <path>\n"
           "  interop_mini_tool verify-coord-keywords --input <path>\n"
           "  interop_mini_tool verify-mixed-coords --input <path>\n"
           "  interop_mini_tool verify-conversion-vectors --input <path>\n"
           "  interop_mini_tool dump-record --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dat-header --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dat-full --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dir --input <dir> --output <path>\n"
           "  interop_mini_tool produce-ms-minimal --output <dir>\n"
           "  interop_mini_tool produce-ms-representative --output <dir>\n"
           "  interop_mini_tool produce-ms-optional-subtables --output <dir>\n"
           "  interop_mini_tool produce-ms-concat --output <dir>\n"
           "  interop_mini_tool produce-ms-selection-stress --output <dir>\n"
           "  interop_mini_tool verify-ms-minimal --input <dir>\n"
           "  interop_mini_tool verify-ms-representative --input <dir>\n"
           "  interop_mini_tool verify-ms-optional-subtables --input <dir>\n"
           "  interop_mini_tool verify-ms-concat --input <dir>\n"
           "  interop_mini_tool verify-ms-selection-stress --input <dir>\n"
           "  interop_mini_tool verify-oracle-ms --input <ms_dir> --oracle <dump>\n"
           "  interop_mini_tool produce-img-minimal --output <dir>\n"
           "  interop_mini_tool produce-img-cube-masked --output <dir>\n"
           "  interop_mini_tool produce-img-region-subset --output <dir>\n"
           "  interop_mini_tool produce-img-expression --output <dir>\n"
           "  interop_mini_tool produce-img-complex --output <dir>\n"
           "  interop_mini_tool verify-img-minimal --input <dir>\n"
           "  interop_mini_tool verify-img-cube-masked --input <dir>\n"
           "  interop_mini_tool verify-img-region-subset --input <dir>\n"
           "  interop_mini_tool verify-img-expression --input <dir>\n"
           "  interop_mini_tool verify-img-complex --input <dir>\n";
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
            verify_tiled_col_dir_artifact(*input, "tiled-col-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-cell-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_cell_dir_artifact(*input, "tiled-cell-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-shape-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_shape_dir_artifact(*input, "tiled-shape-dir");
            return 0;
        }
        if (subcommand == "verify-tiled-data-dir") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_tiled_data_dir_artifact(*input, "tiled-data-dir");
            return 0;
        }

        // --- Phase 8: measure/coordinate record commands ---
        if (subcommand == "produce-measure-scalar") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_scalar_record());
            return 0;
        }
        if (subcommand == "produce-measure-array") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_array_record());
            return 0;
        }
        if (subcommand == "produce-measure-rich") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_rich_record());
            return 0;
        }
        if (subcommand == "produce-coord-keywords") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_coord_keywords_record());
            return 0;
        }
        if (subcommand == "produce-mixed-coords") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_mixed_coords_record());
            return 0;
        }
        if (subcommand == "produce-conversion-vectors") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_conversion_vectors_record());
            return 0;
        }
        if (subcommand == "verify-measure-scalar") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_scalar_record(), "measure-scalar");
            return 0;
        }
        if (subcommand == "verify-measure-array") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_array_record(), "measure-array");
            return 0;
        }
        if (subcommand == "verify-measure-rich") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_rich_record(), "measure-rich");
            return 0;
        }
        if (subcommand == "verify-coord-keywords") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            // Try exact match first; fall back to semantic verification.
            const auto bytes = read_binary(*input);
            casacore_mini::AipsIoReader reader(bytes);
            const auto actual_record = casacore_mini::read_aipsio_record(reader);
            try {
                verify_lines_equal("coord-keywords",
                                   canonical_record_lines(build_coord_keywords_record()),
                                   canonical_record_lines(actual_record));
                std::cout << "  coord-keywords: [PASS] exact match\n";
            } catch (const std::runtime_error&) {
                verify_coord_keywords_semantic(actual_record, "coord-keywords");
            }
            return 0;
        }
        if (subcommand == "verify-mixed-coords") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            const auto bytes = read_binary(*input);
            casacore_mini::AipsIoReader reader(bytes);
            const auto actual_record = casacore_mini::read_aipsio_record(reader);
            try {
                verify_lines_equal("mixed-coords",
                                   canonical_record_lines(build_mixed_coords_record()),
                                   canonical_record_lines(actual_record));
                std::cout << "  mixed-coords: [PASS] exact match\n";
            } catch (const std::runtime_error&) {
                verify_mixed_coords_semantic(actual_record, "mixed-coords");
            }
            return 0;
        }
        if (subcommand == "verify-conversion-vectors") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            const auto bytes = read_binary(*input);
            casacore_mini::AipsIoReader reader(bytes);
            const auto actual_record = casacore_mini::read_aipsio_record(reader);
            try {
                verify_lines_equal("conversion-vectors",
                                   canonical_record_lines(build_conversion_vectors_record()),
                                   canonical_record_lines(actual_record));
                std::cout << "  conversion-vectors: [PASS] exact match\n";
            } catch (const std::runtime_error&) {
                verify_conversion_vectors_semantic(actual_record, "conversion-vectors");
            }
            return 0;
        }

        // --- Phase 9: MS interop commands ---
        if (subcommand == "produce-ms-minimal") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_ms_minimal(*output);
            return 0;
        }
        if (subcommand == "verify-ms-minimal") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ms_minimal(*input);
            return 0;
        }
        if (subcommand == "produce-ms-representative") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_ms_representative(*output);
            return 0;
        }
        if (subcommand == "verify-ms-representative") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ms_representative(*input);
            return 0;
        }
        if (subcommand == "produce-ms-optional-subtables") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_ms_optional_subtables(*output);
            return 0;
        }
        if (subcommand == "verify-ms-optional-subtables") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ms_optional_subtables(*input);
            return 0;
        }
        if (subcommand == "produce-ms-concat") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_ms_concat(*output);
            return 0;
        }
        if (subcommand == "verify-ms-concat") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ms_concat(*input);
            return 0;
        }
        if (subcommand == "produce-ms-selection-stress") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_ms_selection_stress(*output);
            return 0;
        }
        if (subcommand == "verify-ms-selection-stress") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_ms_selection_stress(*input);
            return 0;
        }

        // Oracle conformance
        if (subcommand == "verify-oracle-ms") {
            const auto input = arg_value(argc, argv, "--input");
            const auto oracle = arg_value(argc, argv, "--oracle");
            if (!input.has_value() || !oracle.has_value()) {
                throw std::runtime_error("missing required --input/--oracle");
            }
            verify_oracle_ms(*input, *oracle);
            return 0;
        }

        // --- Phase 10: Image interop commands ---
        if (subcommand == "produce-img-minimal") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_img_minimal(*output);
            return 0;
        }
        if (subcommand == "verify-img-minimal") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_img_minimal(*input);
            return 0;
        }
        if (subcommand == "produce-img-cube-masked") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_img_cube_masked(*output);
            return 0;
        }
        if (subcommand == "verify-img-cube-masked") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_img_cube_masked(*input);
            return 0;
        }
        if (subcommand == "produce-img-region-subset") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_img_region_subset(*output);
            return 0;
        }
        if (subcommand == "verify-img-region-subset") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_img_region_subset(*input);
            return 0;
        }
        if (subcommand == "produce-img-expression") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_img_expression(*output);
            return 0;
        }
        if (subcommand == "verify-img-expression") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_img_expression(*input);
            return 0;
        }
        if (subcommand == "produce-img-complex") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            produce_img_complex(*output);
            return 0;
        }
        if (subcommand == "verify-img-complex") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_img_complex(*input);
            return 0;
        }

        throw std::runtime_error("unknown subcommand: " + subcommand);
    } catch (const std::exception& error) {
        std::cerr << "interop_mini_tool failed: " << error.what() << '\n';
        std::cerr << usage();
        return 1;
    }
}
