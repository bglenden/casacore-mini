#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/aipsio_record_writer.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/table_dat.hpp"
#include "casacore_mini/table_dat_writer.hpp"

#include <algorithm>
#include <complex>
#include <cstdint>
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

[[nodiscard]] std::string usage() {
    return "Usage:\n"
           "  interop_mini_tool write-record-basic --output <path>\n"
           "  interop_mini_tool write-record-nested --output <path>\n"
           "  interop_mini_tool write-table-dat-header --output <path>\n"
           "  interop_mini_tool verify-record-basic --input <path>\n"
           "  interop_mini_tool verify-record-nested --input <path>\n"
           "  interop_mini_tool verify-table-dat-header --input <path>\n"
           "  interop_mini_tool dump-record --input <path> --output <path>\n"
           "  interop_mini_tool dump-table-dat-header --input <path> --output <path>\n";
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
        if (subcommand == "verify-table-dat-header") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_table_dat_header_artifact(*input, "table-dat-header");
            return 0;
        }

        throw std::runtime_error("unknown subcommand: " + subcommand);
    } catch (const std::exception& error) {
        std::cerr << "interop_mini_tool failed: " << error.what() << '\n';
        std::cerr << usage();
        return 1;
    }
}
