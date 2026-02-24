#include <casacore/casa/Arrays/Array.h>
#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Containers/RecordFieldId.h>
#include <casacore/casa/IO/AipsIO.h>
#include <casacore/casa/IO/ByteIO.h>
#include <casacore/casa/Utilities/DataType.h>
#include <casacore/casa/aips.h>

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

[[nodiscard]] std::string format_complex64(const casacore::Complex& value) {
    return "(" + format_hex_float(value.real()) + ";" + format_hex_float(value.imag()) + ")";
}

[[nodiscard]] std::string format_complex128(const casacore::DComplex& value) {
    return "(" + format_hex_float(value.real()) + ";" + format_hex_float(value.imag()) + ")";
}

[[nodiscard]] std::string field_path(const std::string_view prefix,
                                     const std::string_view field_name) {
    if (prefix.empty()) {
        return std::string(field_name);
    }
    return std::string(prefix) + "." + std::string(field_name);
}

[[nodiscard]] std::string join_shape(const casacore::IPosition& shape) {
    std::ostringstream output;
    for (casacore::uInt index = 0; index < shape.nelements(); ++index) {
        if (index > 0U) {
            output << ',';
        }
        output << shape[index];
    }
    return output.str();
}

template <typename array_t, typename formatter_t>
void append_array_line(const std::string_view path, const std::string_view element_type,
                       const array_t& array, formatter_t&& formatter,
                       std::vector<std::string>* lines) {
    std::ostringstream values;
    std::size_t count = 0;
    for (auto it = array.begin(); it != array.end(); ++it, ++count) {
        if (count > 0U) {
            values << ',';
        }
        values << formatter(*it);
    }

    lines->push_back("field=" + std::string(path) + "|array:" + std::string(element_type) +
                     "|shape=" + join_shape(array.shape()) + "|values=" + values.str());
}

void append_record_lines(const casacore::RecordInterface& record, const std::string_view prefix,
                         std::vector<std::string>* lines);

void append_field_line(const casacore::RecordInterface& record,
                       const casacore::RecordFieldId& field_id, const std::string_view path,
                       std::vector<std::string>* lines) {
    const auto type = record.dataType(field_id);
    switch (type) {
    case casacore::TpBool:
        lines->push_back("field=" + std::string(path) + "|bool|" +
                         (record.asBool(field_id) ? "true" : "false"));
        return;
    case casacore::TpShort:
        lines->push_back("field=" + std::string(path) + "|int16|" +
                         std::to_string(record.asShort(field_id)));
        return;
    case casacore::TpUShort: {
        const auto value = static_cast<std::uint64_t>(record.asuInt(field_id));
        lines->push_back("field=" + std::string(path) + "|uint16|" + std::to_string(value));
        return;
    }
    case casacore::TpInt:
        lines->push_back("field=" + std::string(path) + "|int32|" +
                         std::to_string(record.asInt(field_id)));
        return;
    case casacore::TpUInt:
        lines->push_back("field=" + std::string(path) + "|uint32|" +
                         std::to_string(record.asuInt(field_id)));
        return;
    case casacore::TpInt64:
        lines->push_back("field=" + std::string(path) + "|int64|" +
                         std::to_string(record.asInt64(field_id)));
        return;
    case casacore::TpFloat:
        lines->push_back("field=" + std::string(path) + "|float32|" +
                         format_hex_float(record.asFloat(field_id)));
        return;
    case casacore::TpDouble:
        lines->push_back("field=" + std::string(path) + "|float64|" +
                         format_hex_float(record.asDouble(field_id)));
        return;
    case casacore::TpComplex:
        lines->push_back("field=" + std::string(path) + "|complex64|" +
                         format_complex64(record.asComplex(field_id)));
        return;
    case casacore::TpDComplex:
        lines->push_back("field=" + std::string(path) + "|complex128|" +
                         format_complex128(record.asDComplex(field_id)));
        return;
    case casacore::TpString:
        lines->push_back("field=" + std::string(path) +
                         "|string|b64:" + base64_encode(record.asString(field_id)));
        return;
    case casacore::TpRecord:
        append_record_lines(record.asRecord(field_id), path, lines);
        return;
    case casacore::TpArrayShort:
        append_array_line(
            path, "int16", record.asArrayShort(field_id),
            [](const casacore::Short value) { return std::to_string(value); }, lines);
        return;
    case casacore::TpArrayInt:
        append_array_line(
            path, "int32", record.asArrayInt(field_id),
            [](const casacore::Int value) { return std::to_string(value); }, lines);
        return;
    case casacore::TpArrayUInt:
        append_array_line(
            path, "uint32", record.asArrayuInt(field_id),
            [](const casacore::uInt value) { return std::to_string(value); }, lines);
        return;
    case casacore::TpArrayInt64:
        append_array_line(
            path, "int64", record.asArrayInt64(field_id),
            [](const casacore::Int64 value) { return std::to_string(value); }, lines);
        return;
    case casacore::TpArrayFloat:
        append_array_line(
            path, "float32", record.asArrayFloat(field_id),
            [](const casacore::Float value) { return format_hex_float(value); }, lines);
        return;
    case casacore::TpArrayDouble:
        append_array_line(
            path, "float64", record.asArrayDouble(field_id),
            [](const casacore::Double value) { return format_hex_float(value); }, lines);
        return;
    case casacore::TpArrayComplex:
        append_array_line(
            path, "complex64", record.asArrayComplex(field_id),
            [](const casacore::Complex& value) { return format_complex64(value); }, lines);
        return;
    case casacore::TpArrayDComplex:
        append_array_line(
            path, "complex128", record.asArrayDComplex(field_id),
            [](const casacore::DComplex& value) { return format_complex128(value); }, lines);
        return;
    case casacore::TpArrayString:
        append_array_line(
            path, "string", record.asArrayString(field_id),
            [](const casacore::String& value) { return "b64:" + base64_encode(value); }, lines);
        return;
    default:
        throw std::runtime_error("unsupported Record field DataType in dump: " +
                                 std::to_string(static_cast<int>(type)));
    }
}

void append_record_lines(const casacore::RecordInterface& record, const std::string_view prefix,
                         std::vector<std::string>* lines) {
    for (casacore::uInt index = 0; index < record.nfields(); ++index) {
        const casacore::RecordFieldId field_id(static_cast<int>(index));
        const auto name = std::string(record.name(field_id));
        append_field_line(record, field_id, field_path(prefix, name), lines);
    }
}

[[nodiscard]] std::vector<std::string>
canonical_record_lines(const casacore::RecordInterface& record) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=record");
    append_record_lines(record, "", &lines);
    return lines;
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

[[nodiscard]] casacore::Record build_record_basic() {
    casacore::Record record;
    record.define("flag", true);
    record.define("i16", static_cast<casacore::Short>(-42));
    record.define("i32", static_cast<casacore::Int>(-100000));
    record.define("u32", static_cast<casacore::uInt>(200000));
    record.define("i64", static_cast<casacore::Int64>(9000000000LL));
    record.define("f32", static_cast<casacore::Float>(1.5F));
    record.define("f64", static_cast<casacore::Double>(3.14));
    record.define("c64", casacore::Complex(1.0F, -2.0F));
    record.define("c128", casacore::DComplex(3.0, -4.0));
    record.define("label", casacore::String("hello"));

    casacore::Array<casacore::Int> arr_i32(casacore::IPosition(2, 2, 2));
    {
        auto it = arr_i32.begin();
        *it++ = 10;
        *it++ = 20;
        *it++ = 30;
        *it++ = 40;
    }
    record.define("arr_i32", arr_i32);

    casacore::Array<casacore::Double> arr_f64(casacore::IPosition(2, 2, 3));
    {
        auto it = arr_f64.begin();
        *it++ = 1.0;
        *it++ = 2.0;
        *it++ = 3.0;
        *it++ = 4.0;
        *it++ = 5.0;
        *it++ = 6.0;
    }
    record.define("arr_f64", arr_f64);

    casacore::Array<casacore::Complex> arr_c64(casacore::IPosition(1, 2));
    {
        auto it = arr_c64.begin();
        *it++ = casacore::Complex(1.0F, 2.0F);
        *it++ = casacore::Complex(3.0F, 4.0F);
    }
    record.define("arr_c64", arr_c64);

    casacore::Array<casacore::String> arr_str(casacore::IPosition(1, 3));
    {
        auto it = arr_str.begin();
        *it++ = casacore::String("alpha");
        *it++ = casacore::String("beta");
        *it++ = casacore::String("gamma");
    }
    record.define("arr_str", arr_str);

    return record;
}

[[nodiscard]] casacore::Record build_record_nested() {
    casacore::Record child;
    child.define("pi", static_cast<casacore::Double>(3.14159));
    child.define("name", casacore::String("test"));

    casacore::Array<casacore::Int64> vec_i64(casacore::IPosition(1, 3));
    {
        auto it = vec_i64.begin();
        *it++ = 7;
        *it++ = 8;
        *it++ = 9;
    }
    child.define("vec_i64", vec_i64);

    casacore::Record record;
    record.defineRecord("child", child, casacore::RecordInterface::Variable);
    record.define("value", static_cast<casacore::Int>(42));

    casacore::Array<casacore::String> units(casacore::IPosition(1, 2));
    {
        auto it = units.begin();
        *it++ = casacore::String("m");
        *it++ = casacore::String("s");
    }
    record.define("units", units);

    return record;
}

[[nodiscard]] casacore::uInt expected_table_version() {
    return 2U;
}

[[nodiscard]] std::uint64_t expected_row_count() {
    return 375U;
}

[[nodiscard]] bool expected_big_endian() {
    return false;
}

[[nodiscard]] casacore::String expected_table_type() {
    return casacore::String("PlainTable");
}

void write_record_artifact(const std::filesystem::path& output_path,
                           const casacore::Record& record) {
    std::filesystem::create_directories(output_path.parent_path());
    casacore::AipsIO output(casacore::String(output_path.string()), casacore::ByteIO::New);
    output << record;
}

void dump_record_artifact(const std::filesystem::path& input_path,
                          const std::filesystem::path& output_path) {
    casacore::AipsIO input(casacore::String(input_path.string()));
    casacore::Record record;
    input >> record;

    write_text(output_path, canonical_record_lines(record));
}

void verify_record_artifact(const std::filesystem::path& input_path,
                            const casacore::Record& expected_record, const std::string_view label) {
    casacore::AipsIO input(casacore::String(input_path.string()));
    casacore::Record actual_record;
    input >> actual_record;
    verify_lines_equal(label, canonical_record_lines(expected_record),
                       canonical_record_lines(actual_record));
}

void write_table_dat_header_artifact(const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    casacore::AipsIO output(casacore::String(output_path.string()), casacore::ByteIO::New);
    output.putstart("Table", expected_table_version());
    output << static_cast<casacore::uInt>(expected_row_count());
    output << static_cast<casacore::uInt>(1U); // little-endian table flag
    output << expected_table_type();
    output.putend();
}

[[nodiscard]] std::vector<std::string>
canonical_table_dat_lines(const casacore::uInt table_version, const std::uint64_t row_count,
                          const bool big_endian, const casacore::String& table_type) {
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dat_header");
    lines.emplace_back("table_version=" + std::to_string(table_version));
    lines.emplace_back("row_count=" + std::to_string(row_count));
    lines.emplace_back(std::string("big_endian=") + (big_endian ? "true" : "false"));
    lines.emplace_back("table_type_b64=" + base64_encode(table_type));
    return lines;
}

void dump_table_dat_header_artifact(const std::filesystem::path& input_path,
                                    const std::filesystem::path& output_path) {
    casacore::AipsIO input(casacore::String(input_path.string()));
    const auto table_version = input.getstart("Table");
    std::uint64_t row_count = 0;
    if (table_version <= 2U) {
        casacore::uInt row_count_u32 = 0;
        input >> row_count_u32;
        row_count = row_count_u32;
    } else {
        casacore::uInt64 row_count_u64 = 0;
        input >> row_count_u64;
        row_count = row_count_u64;
    }
    casacore::uInt endian_flag = 0;
    casacore::String table_type;
    input >> endian_flag;
    input >> table_type;
    input.getend();
    write_text(output_path,
               canonical_table_dat_lines(table_version, row_count, endian_flag == 0U, table_type));
}

void verify_table_dat_header_artifact(const std::filesystem::path& input_path,
                                      const std::string_view label) {
    casacore::AipsIO input(casacore::String(input_path.string()));
    const auto table_version = input.getstart("Table");
    std::uint64_t row_count = 0;
    if (table_version <= 2U) {
        casacore::uInt row_count_u32 = 0;
        input >> row_count_u32;
        row_count = row_count_u32;
    } else {
        casacore::uInt64 row_count_u64 = 0;
        input >> row_count_u64;
        row_count = row_count_u64;
    }
    casacore::uInt endian_flag = 0;
    casacore::String table_type;
    input >> endian_flag;
    input >> table_type;
    input.getend();

    verify_lines_equal(
        label,
        canonical_table_dat_lines(expected_table_version(), expected_row_count(),
                                  expected_big_endian(), expected_table_type()),
        canonical_table_dat_lines(table_version, row_count, endian_flag == 0U, table_type));
}

[[nodiscard]] std::string usage() {
    return "Usage:\n"
           "  casacore_interop_tool write-record-basic --output <path>\n"
           "  casacore_interop_tool write-record-nested --output <path>\n"
           "  casacore_interop_tool write-table-dat-header --output <path>\n"
           "  casacore_interop_tool verify-record-basic --input <path>\n"
           "  casacore_interop_tool verify-record-nested --input <path>\n"
           "  casacore_interop_tool verify-table-dat-header --input <path>\n"
           "  casacore_interop_tool dump-record --input <path> --output <path>\n"
           "  casacore_interop_tool dump-table-dat-header --input <path> --output <path>\n";
}

} // namespace

int main(int argc, char** argv) {
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
            dump_record_artifact(*input, *output);
            return 0;
        }
        if (subcommand == "dump-table-dat-header") {
            const auto input = arg_value(argc, argv, "--input");
            const auto output = arg_value(argc, argv, "--output");
            if (!input.has_value() || !output.has_value()) {
                throw std::runtime_error("missing required --input/--output");
            }
            dump_table_dat_header_artifact(*input, *output);
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
        std::cerr << "casacore_interop_tool failed: " << error.what() << '\n';
        std::cerr << usage();
        return 1;
    }
}
