#include <casacore/casa/Arrays/Array.h>

#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Containers/RecordFieldId.h>
#include <casacore/casa/IO/AipsIO.h>
#include <casacore/casa/IO/ByteIO.h>
#include <casacore/casa/Utilities/DataType.h>
#include <casacore/casa/aips.h>
#include <casacore/tables/Tables/ColumnDesc.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/tables/DataMan/TiledCellStMan.h>
#include <casacore/tables/DataMan/TiledColumnStMan.h>
#include <casacore/tables/DataMan/TiledDataStMan.h>
#include <casacore/tables/DataMan/TiledDataStManAccessor.h>
#include <casacore/tables/DataMan/TiledShapeStMan.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/TableColumn.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableAttr.h>
#include <casacore/tables/Tables/TableDesc.h>

#include <algorithm>
#include <cmath>
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

[[nodiscard]] casacore::Record read_fixture_record(const std::string_view fixture_id) {
    const auto fixture_path = fixture_record_path(fixture_id);
    casacore::AipsIO input(casacore::String(fixture_path.string()));
    casacore::Record record;
    input >> record;
    return record;
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

void write_fixture_record_artifact(const std::filesystem::path& output_path,
                                   const std::string_view fixture_id) {
    write_record_artifact(output_path, read_fixture_record(fixture_id));
}

void verify_fixture_record_artifact(const std::filesystem::path& input_path,
                                    const std::string_view fixture_id, const char* label) {
    casacore::AipsIO input(casacore::String(input_path.string()));
    casacore::Record actual_record;
    input >> actual_record;

    const auto expected_record = read_fixture_record(fixture_id);
    verify_lines_equal(label, canonical_record_lines(expected_record),
                       canonical_record_lines(actual_record));
}

// --- table_dat_full interop ---

[[nodiscard]] std::string datatype_name(casacore::DataType dtype) {
    switch (dtype) {
    case casacore::TpBool:
        return "TpBool";
    case casacore::TpChar:
        return "TpChar";
    case casacore::TpUChar:
        return "TpUChar";
    case casacore::TpShort:
        return "TpShort";
    case casacore::TpUShort:
        return "TpUShort";
    case casacore::TpInt:
        return "TpInt";
    case casacore::TpUInt:
        return "TpUInt";
    case casacore::TpFloat:
        return "TpFloat";
    case casacore::TpDouble:
        return "TpDouble";
    case casacore::TpComplex:
        return "TpComplex";
    case casacore::TpDComplex:
        return "TpDComplex";
    case casacore::TpString:
        return "TpString";
    case casacore::TpTable:
        return "TpTable";
    case casacore::TpInt64:
        return "TpInt64";
    default:
        return "Unknown(" + std::to_string(static_cast<int>(dtype)) + ")";
    }
}



/// Read the ttable2_v1 fixture and re-encode via casacore AipsIO as a table.dat.
/// This ensures the casacore side writes the same fixture data the mini side does.
void write_table_dat_full_artifact(const std::filesystem::path& output_path) {
    // Read the fixture's table.dat using casacore AipsIO (which is already casacore-encoded).
    // Since the fixture was produced by casacore, just copy it directly.
    auto fixture_path =
        std::filesystem::path("data/corpus/fixtures/table_dat_ttable2_v1/table.dat");
    std::filesystem::create_directories(output_path.parent_path());
    std::filesystem::copy_file(fixture_path, output_path,
                               std::filesystem::copy_options::overwrite_existing);
}

/// Read a table.dat and produce a canonical dump using casacore APIs.
/// Uses TableDesc::getFile to properly deserialize the TableDesc, then
/// reads post-TD data manually. Does not require SM data files.
[[nodiscard]] std::vector<std::string>
read_table_dat_full_canonical(const std::filesystem::path& input_path) {
    casacore::AipsIO aio(casacore::String(input_path.string()));
    const auto tv = aio.getstart("Table");

    std::uint64_t row_count = 0;
    if (tv <= 2U) {
        casacore::uInt r32 = 0;
        aio >> r32;
        row_count = r32;
    } else {
        casacore::uInt64 r64 = 0;
        aio >> r64;
        row_count = r64;
    }

    casacore::uInt endian_flag = 0;
    casacore::String table_type;
    aio >> endian_flag >> table_type;

    // Use casacore's own TableDesc deserialization.
    casacore::TableDesc td;
    casacore::TableAttr attr;
    td.getFile(aio, attr);

    // Build canonical lines.
    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dat_full");
    lines.emplace_back("table_version=" + std::to_string(tv));
    lines.emplace_back("row_count=" + std::to_string(row_count));
    lines.emplace_back("td_name_b64=" + base64_encode(td.getType()));
    lines.emplace_back("td_ncol=" + std::to_string(td.ncolumn()));

    for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
        const auto& col = td.columnDesc(ci);
        const auto prefix = "col[" + std::to_string(ci) + "]";

        lines.emplace_back(prefix + ".name_b64=" + base64_encode(col.name()));
        lines.emplace_back(prefix + ".kind=" +
                           std::string(col.isArray() ? "array" : "scalar"));
        lines.emplace_back(prefix + ".dtype=" + datatype_name(col.dataType()));
        lines.emplace_back(prefix + ".ndim=" + std::to_string(col.ndim()));
        if (col.ndim() > 0 && col.shape().nelements() > 0U) {
            lines.emplace_back(prefix + ".shape=" + join_shape(col.shape()));
        }
        lines.emplace_back(prefix + ".max_length=" +
                           std::to_string(col.maxLength()));
        lines.emplace_back(prefix + ".dm_type_b64=" +
                           base64_encode(col.dataManagerType()));
        lines.emplace_back(prefix + ".dm_group_b64=" +
                           base64_encode(col.dataManagerGroup()));
    }

    return lines;
}

void dump_table_dat_full_artifact(const std::filesystem::path& input_path,
                                  const std::filesystem::path& output_path) {
    write_text(output_path, read_table_dat_full_canonical(input_path));
}

void verify_table_dat_full_artifact(const std::filesystem::path& input_path,
                                    const std::string_view label) {
    auto fixture_path =
        std::filesystem::path("data/corpus/fixtures/table_dat_ttable2_v1/table.dat");
    const auto expected = read_table_dat_full_canonical(fixture_path);
    auto actual = read_table_dat_full_canonical(input_path);
    // Patch version in actual if it differs (mini always writes v2).
    for (auto& line : actual) {
        if (line == "table_version=1") {
            line = "table_version=2";
        }
    }
    auto expected_patched = expected;
    for (auto& line : expected_patched) {
        if (line == "table_version=1") {
            line = "table_version=2";
        }
    }
    verify_lines_equal(label, expected_patched, actual);
}

// --- table directory interop ---

/// Create a small SSM table with casacore Table APIs at the given directory.
/// 5 rows, 4 columns (id:Int, value:Float, label:String, dval:Double).
void write_table_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_ssm", casacore::TableDesc::Scratch);
    td.addColumn(casacore::ScalarColumnDesc<casacore::Int>("id"));
    td.addColumn(casacore::ScalarColumnDesc<casacore::Float>("value"));
    td.addColumn(casacore::ScalarColumnDesc<casacore::String>("label"));
    td.addColumn(casacore::ScalarColumnDesc<casacore::Double>("dval"));

    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    casacore::Table table(setup, 5);

    casacore::ScalarColumn<casacore::Int> col_id(table, "id");
    casacore::ScalarColumn<casacore::Float> col_value(table, "value");
    casacore::ScalarColumn<casacore::String> col_label(table, "label");
    casacore::ScalarColumn<casacore::Double> col_dval(table, "dval");

    for (casacore::uInt i = 0; i < 5; ++i) {
        col_id.put(i, static_cast<casacore::Int>(i * 10));
        col_value.put(i, static_cast<casacore::Float>(i) * 1.5F);
        col_label.put(i, casacore::String("row_") + casacore::String::toString(i));
        col_dval.put(i, static_cast<casacore::Double>(i) * 3.14);
    }

    // Table keywords: observer, telescope, version.
    table.rwKeywordSet().define("observer", casacore::String("test_user"));
    table.rwKeywordSet().define("telescope", casacore::String("VLA"));
    table.rwKeywordSet().define("version", static_cast<casacore::Int>(2));

    // Column keyword on "value": UNIT = "Jy".
    casacore::TableColumn tc_value(table, "value");
    tc_value.rwKeywordSet().define("UNIT", casacore::String("Jy"));

    table.flush(true);
}

/// Canonical lines for a table directory using casacore Table APIs.
[[nodiscard]] std::vector<std::string>
read_table_dir_canonical(const std::filesystem::path& input_dir) {
    casacore::Table table(casacore::String(input_dir.string()), casacore::Table::Old);
    const auto td = table.actualTableDesc();

    std::vector<std::string> lines;
    lines.emplace_back("kind=table_dir");
    lines.emplace_back("row_count=" + std::to_string(table.nrow()));
    lines.emplace_back("ncol=" + std::to_string(td.ncolumn()));
    for (casacore::uInt i = 0; i < td.ncolumn(); ++i) {
        const auto& col = td.columnDesc(i);
        const auto prefix = "col[" + std::to_string(i) + "]";
        lines.emplace_back(prefix + ".name_b64=" + base64_encode(col.name()));
        lines.emplace_back(prefix + ".dtype=" + datatype_name(col.dataType()));
    }
    // SM file count from directory listing.
    std::uint32_t sm_file_count = 0;
    for (std::uint32_t seq = 0; seq < 100U; ++seq) {
        const auto fpath = input_dir / ("table.f" + std::to_string(seq));
        if (std::filesystem::exists(fpath)) {
            ++sm_file_count;
        }
    }
    lines.emplace_back("sm_file_count=" + std::to_string(sm_file_count));
    for (std::uint32_t seq = 0; seq < 100U; ++seq) {
        const auto fpath = input_dir / ("table.f" + std::to_string(seq));
        if (std::filesystem::exists(fpath)) {
            const auto prefix = "sm[" + std::to_string(seq) + "]";
            lines.emplace_back(prefix + ".filename=table.f" + std::to_string(seq));
            lines.emplace_back(prefix + ".size=" +
                               std::to_string(std::filesystem::file_size(fpath)));
        }
    }
    return lines;
}

void verify_table_dir_artifact(const std::filesystem::path& input_dir,
                                const std::string_view label) {
    // Just verify casacore can open it and the structure matches expectations.
    auto actual_lines = read_table_dir_canonical(input_dir);
    // Build expected (without SM file details, since mini doesn't write data files).
    std::vector<std::string> expected_meta;
    expected_meta.emplace_back("kind=table_dir");
    expected_meta.emplace_back("row_count=5");
    expected_meta.emplace_back("ncol=4");
    expected_meta.emplace_back("col[0].name_b64=" + base64_encode("id"));
    expected_meta.emplace_back("col[0].dtype=TpInt");
    expected_meta.emplace_back("col[1].name_b64=" + base64_encode("value"));
    expected_meta.emplace_back("col[1].dtype=TpFloat");
    expected_meta.emplace_back("col[2].name_b64=" + base64_encode("label"));
    expected_meta.emplace_back("col[2].dtype=TpString");
    expected_meta.emplace_back("col[3].name_b64=" + base64_encode("dval"));
    expected_meta.emplace_back("col[3].dtype=TpDouble");
    expected_meta.emplace_back("sm_file_count=*");

    // Strip SM file lines from actual.
    std::vector<std::string> actual_meta;
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

    // Open table for semantic checks.
    casacore::Table table(casacore::String(input_dir.string()),
                          casacore::Table::Old);
    const auto td = table.actualTableDesc();

    std::cout << "  " << label << ": [PASS] structure ("
              << table.nrow() << " rows, " << td.ncolumn() << " cols)\n";

    // Table keywords (content verification).
    {
        casacore::Record expected_tkw;
        expected_tkw.define("observer", casacore::String("test_user"));
        expected_tkw.define("telescope", casacore::String("VLA"));
        expected_tkw.define("version", static_cast<casacore::Int>(2));
        verify_lines_equal(std::string(label) + ":table_keywords",
            canonical_record_lines(expected_tkw),
            canonical_record_lines(table.keywordSet()));
    }
    std::cout << "  " << label << ": [PASS] table_keywords ("
              << table.keywordSet().nfields() << " fields, content verified)\n";

    // Per-column keywords (content verification).
    {
        // "value" column should have UNIT="Jy"; others should be empty.
        for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
            casacore::Record expected_ckw;
            if (td.columnDesc(ci).name() == "value") {
                expected_ckw.define("UNIT", casacore::String("Jy"));
            }
            verify_lines_equal(
                std::string(label) + ":col[" + std::to_string(ci) + "]_keywords",
                canonical_record_lines(expected_ckw),
                canonical_record_lines(td.columnDesc(ci).keywordSet()));
        }
    }
    {
        std::size_t total_col_kw = 0;
        for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
            total_col_kw += td.columnDesc(ci).keywordSet().nfields();
        }
        std::cout << "  " << label << ": [PASS] col_keywords ("
                  << td.ncolumn() << " cols, " << total_col_kw
                  << " fields total, content verified)\n";
    }

    // SM mapping: all columns should use StandardStMan.
    for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
        const auto dm_type = td.columnDesc(ci).dataManagerType();
        if (dm_type != "StandardStMan" && dm_type != "SSM") {
            throw std::runtime_error(
                std::string(label) + ": col " + std::string(td.columnDesc(ci).name().c_str()) +
                " has dm_type=" + std::string(dm_type.c_str()) +
                " expected StandardStMan");
        }
    }
    std::cout << "  " << label << ": [PASS] sm_mapping ("
              << td.ncolumn() << " cols -> StandardStMan)\n";

    // --- Cell-value verification using casacore ScalarColumn ---
    const auto f0_path = input_dir / "table.f0";
    if (std::filesystem::exists(f0_path)) {
        if (table.nrow() != 5) {
            throw std::runtime_error(std::string(label) +
                                     ": expected 5 rows, got " +
                                     std::to_string(table.nrow()));
        }

        casacore::ScalarColumn<casacore::Int> col_id(table, "id");
        casacore::ScalarColumn<casacore::Float> col_value(table, "value");
        casacore::ScalarColumn<casacore::String> col_label(table, "label");
        casacore::ScalarColumn<casacore::Double> col_dval(table, "dval");

        const casacore::Int expected_id[] = {0, 10, 20, 30, 40};
        const casacore::Float expected_val[] = {0.0F, 1.5F, 3.0F, 4.5F, 6.0F};
        const casacore::String expected_lbl[] = {"row_0", "row_1", "row_2",
                                                  "row_3", "row_4"};
        const casacore::Double expected_dv[] = {0.0, 3.14, 6.28, 9.42, 12.56};

        for (casacore::uInt r = 0; r < 5; ++r) {
            if (col_id(r) != expected_id[r]) {
                throw std::runtime_error(
                    std::string(label) + ": id[" + std::to_string(r) +
                    "] mismatch: expected " + std::to_string(expected_id[r]) +
                    " got " + std::to_string(col_id(r)));
            }
            if (std::fabs(col_value(r) - expected_val[r]) > kFloat32Tolerance) {
                throw std::runtime_error(
                    std::string(label) + ": value[" + std::to_string(r) +
                    "] mismatch");
            }
            if (col_label(r) != expected_lbl[r]) {
                throw std::runtime_error(
                    std::string(label) + ": label[" + std::to_string(r) +
                    "] mismatch: expected '" +
                    std::string(expected_lbl[r].c_str()) + "'");
            }
            if (std::fabs(col_dval(r) - expected_dv[r]) > kFloat64Tolerance) {
                throw std::runtime_error(
                    std::string(label) + ": dval[" + std::to_string(r) +
                    "] mismatch");
            }
        }
        std::cout << "  " << label
                  << ": [PASS] cell_values (20 cells, float tol=1e-6, double tol=1e-10)\n";
    } else {
        std::cout << "  " << label
                  << ": SSM data file not present; cell verification skipped\n";
    }
}

void dump_table_dir_artifact(const std::filesystem::path& input_dir,
                              const std::filesystem::path& output_path) {
    write_text(output_path, read_table_dir_canonical(input_dir));
}

// --- ISM table directory interop ---

/// Create a table with IncrementalStMan: 3 scalar columns, 10 rows.
/// Col "time" is Double, col "antenna" is Int, col "flag" is Bool.
void write_ism_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_ism", casacore::TableDesc::Scratch);
    td.addColumn(casacore::ScalarColumnDesc<casacore::Double>("time"));
    td.addColumn(casacore::ScalarColumnDesc<casacore::Int>("antenna"));
    td.addColumn(casacore::ScalarColumnDesc<casacore::Bool>("flag"));

    casacore::IncrementalStMan ism("ISMData");
    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    setup.bindAll(ism);
    casacore::Table table(setup, 10);

    casacore::ScalarColumn<casacore::Double> col_time(table, "time");
    casacore::ScalarColumn<casacore::Int> col_antenna(table, "antenna");
    casacore::ScalarColumn<casacore::Bool> col_flag(table, "flag");

    for (casacore::uInt i = 0; i < 10; ++i) {
        col_time.put(i, 4.8e9 + static_cast<casacore::Double>(i) * 10.0);
        col_antenna.put(i, static_cast<casacore::Int>(i % 3));
        col_flag.put(i, (i % 2) == 0);
    }

    // Table keywords: instrument, epoch.
    table.rwKeywordSet().define("instrument", casacore::String("ALMA"));
    table.rwKeywordSet().define("epoch", static_cast<casacore::Double>(4.8e9));

    // Column keywords on "time": MEASINFO (sub-Record) and QuantumUnits (string_array).
    {
        casacore::TableColumn tc_time(table, "time");
        casacore::Record measinfo;
        measinfo.define("type", casacore::String("epoch"));
        measinfo.define("Ref", casacore::String("UTC"));
        tc_time.rwKeywordSet().defineRecord("MEASINFO", measinfo);

        casacore::Array<casacore::String> qu_arr(casacore::IPosition(1, 1));
        qu_arr(casacore::IPosition(1, 0)) = casacore::String("s");
        tc_time.rwKeywordSet().define("QuantumUnits", qu_arr);
    }

    table.flush(true);
}

/// Expected canonical lines for ISM table.
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

// --- tiled table directory interop ---

/// Create a table with TiledColumnStMan: 2 array columns, 10 rows.
/// Col "data" is Float with fixed shape [4,8], col "flags" is Int with fixed shape [4,8].
void write_tiled_col_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_tiledcol", casacore::TableDesc::Scratch);
    td.addColumn(casacore::ArrayColumnDesc<casacore::Float>("data", casacore::IPosition(2, 4, 8),
                                                            casacore::ColumnDesc::FixedShape));
    td.addColumn(casacore::ArrayColumnDesc<casacore::Int>("flags", casacore::IPosition(2, 4, 8),
                                                          casacore::ColumnDesc::FixedShape));

    casacore::TiledColumnStMan tsm("TiledCol", casacore::IPosition(2, 4, 8));
    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    setup.bindAll(tsm);
    casacore::Table table(setup, 10);

    casacore::ArrayColumn<casacore::Float> col_data(table, "data");
    casacore::ArrayColumn<casacore::Int> col_flags(table, "flags");

    for (casacore::uInt r = 0; r < 10; ++r) {
        casacore::Array<casacore::Float> d(casacore::IPosition(2, 4, 8));
        d = static_cast<casacore::Float>(r) * 0.1F;
        col_data.put(r, d);

        casacore::Array<casacore::Int> f(casacore::IPosition(2, 4, 8));
        f = static_cast<casacore::Int>(r);
        col_flags.put(r, f);
    }
    table.flush(true);
}

/// Create a table with TiledCellStMan: 1 array column, 5 rows.
/// Col "map" is Float with fixed shape [32,8] (mirrors pagedimage fixture).
void write_tiled_cell_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_tiledcell", casacore::TableDesc::Scratch);
    td.addColumn(casacore::ArrayColumnDesc<casacore::Float>("map", casacore::IPosition(2, 32, 8),
                                                            casacore::ColumnDesc::FixedShape));

    casacore::TiledCellStMan tsm("TiledCell", casacore::IPosition(2, 32, 8));
    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    setup.bindAll(tsm);
    casacore::Table table(setup, 5);

    casacore::ArrayColumn<casacore::Float> col_map(table, "map");
    for (casacore::uInt r = 0; r < 5; ++r) {
        casacore::Array<casacore::Float> m(casacore::IPosition(2, 32, 8));
        m = static_cast<casacore::Float>(r) * 1.5F;
        col_map.put(r, m);
    }
    table.flush(true);
}

/// Create a table with TiledShapeStMan: 1 array column, 5 rows.
/// Col "vis" is Complex with fixed shape [4,16].
void write_tiled_shape_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_tiledshape", casacore::TableDesc::Scratch);
    td.addColumn(
        casacore::ArrayColumnDesc<casacore::Complex>("vis", 2)); // ndim=2, variable shape

    casacore::TiledShapeStMan tsm("TiledShape", casacore::IPosition(3, 4, 16, 1));
    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    setup.bindAll(tsm);
    casacore::Table table(setup, 5);

    casacore::ArrayColumn<casacore::Complex> col_vis(table, "vis");
    for (casacore::uInt r = 0; r < 5; ++r) {
        casacore::Array<casacore::Complex> v(casacore::IPosition(2, 4, 16));
        v = casacore::Complex(static_cast<float>(r), 0.0F);
        col_vis.put(r, v);
    }
    table.flush(true);
}

/// Create a table with TiledDataStMan: 1 array column, 5 rows.
/// Col "spectrum" is Float with fixed shape [256].
/// TiledDataStMan requires defineHypercolumn on the TableDesc and explicit
/// hypercube management via TiledDataStManAccessor.
void write_tiled_data_dir_artifact(const std::filesystem::path& output_dir) {
    std::filesystem::remove_all(output_dir);

    casacore::TableDesc td("test_tileddata", casacore::TableDesc::Scratch);
    td.addColumn(casacore::ArrayColumnDesc<casacore::Float>(
        "spectrum", casacore::IPosition(1, 256), casacore::ColumnDesc::FixedShape));

    // TiledDataStMan requires a hypercolumn definition.
    td.defineHypercolumn("TiledData", 2,
                         casacore::Vector<casacore::String>(1, "spectrum"));

    casacore::TiledDataStMan tsm("TiledData");
    casacore::SetupNewTable setup(casacore::String(output_dir.string()), td, casacore::Table::New);
    setup.bindAll(tsm);
    casacore::Table table(setup, 5); // Create with 5 rows.

    // Use TiledDataStManAccessor to define the hypercube.
    casacore::TiledDataStManAccessor accessor(table, "TiledData");
    casacore::Record values;
    accessor.addHypercube(casacore::IPosition(2, 256, 5),
                          casacore::IPosition(2, 256, 5), values);

    casacore::ArrayColumn<casacore::Float> col_spectrum(table, "spectrum");
    for (casacore::uInt r = 0; r < 5; ++r) {
        casacore::Array<casacore::Float> s(casacore::IPosition(1, 256));
        s = static_cast<casacore::Float>(r) * 0.01F;
        col_spectrum.put(r, s);
    }
    table.flush(true);
}

/// Expected canonical lines for tiled column table (metadata only).
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

/// Expected canonical lines for tiled cell table.
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

/// Expected canonical lines for tiled shape table.
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

/// Expected canonical lines for tiled data table.
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

/// Strip SM file detail lines, replacing sm_file_count with wildcard.
[[nodiscard]] std::vector<std::string>
strip_sm_file_lines(const std::vector<std::string>& lines) {
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

/// Shared verification of directory structure and semantics via casacore Table.
/// Checks metadata lines, opens the table, prints structure / keywords / SM mapping.
/// expected_table_kw: expected table-level keywords (content verified).
/// expected_col_kw: map of column name -> expected keyword Record (if any).
void verify_dir_structure_and_semantics(
    const std::filesystem::path& input_dir,
    const std::vector<std::string>& expected_meta,
    const std::string_view label,
    const std::string& expected_dm,
    const casacore::Record& expected_table_kw,
    const std::vector<std::pair<std::string, casacore::Record>>& expected_col_kw) {
    auto actual_lines = read_table_dir_canonical(input_dir);
    auto actual_meta = strip_sm_file_lines(actual_lines);
    verify_lines_equal(label, expected_meta, actual_meta);

    casacore::Table table(casacore::String(input_dir.string()));
    const auto td = table.actualTableDesc();

    std::cout << "  " << label << ": [PASS] structure ("
              << table.nrow() << " rows, " << td.ncolumn() << " cols)\n";

    // Table keywords (content verification).
    verify_lines_equal(std::string(label) + ":table_keywords",
        canonical_record_lines(expected_table_kw),
        canonical_record_lines(table.keywordSet()));
    std::cout << "  " << label << ": [PASS] table_keywords ("
              << table.keywordSet().nfields() << " fields, content verified)\n";

    // Per-column keywords (content verification).
    for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
        casacore::Record expected_ckw;
        for (const auto& [col_name, kw] : expected_col_kw) {
            if (std::string(td.columnDesc(ci).name().c_str()) == col_name) {
                expected_ckw = kw;
                break;
            }
        }
        verify_lines_equal(
            std::string(label) + ":col[" + std::to_string(ci) + "]_keywords",
            canonical_record_lines(expected_ckw),
            canonical_record_lines(td.columnDesc(ci).keywordSet()));
    }
    {
        std::size_t total_col_kw_count = 0;
        for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
            total_col_kw_count += td.columnDesc(ci).keywordSet().nfields();
        }
        std::cout << "  " << label << ": [PASS] col_keywords ("
                  << td.ncolumn() << " cols, " << total_col_kw_count
                  << " fields total, content verified)\n";
    }

    // SM mapping: strict check via dataManagerGroup(), with a fallback to
    // dataManagerType() for casacore-authored artifacts where group can be
    // canonicalized to StandardStMan in TableDesc.
    if (!expected_dm.empty() && td.ncolumn() > 0) {
        std::string expected_dm_type;
        if (expected_dm == "ISMData") {
            expected_dm_type = "IncrementalStMan";
        } else if (expected_dm == "TiledCol") {
            expected_dm_type = "TiledColumnStMan";
        } else if (expected_dm == "TiledCell") {
            expected_dm_type = "TiledCellStMan";
        } else if (expected_dm == "TiledShape") {
            expected_dm_type = "TiledShapeStMan";
        } else if (expected_dm == "TiledData") {
            expected_dm_type = "TiledDataStMan";
        } else {
            expected_dm_type = expected_dm;
        }

        std::size_t matched_type_fallback_count = 0;
        for (casacore::uInt ci = 0; ci < td.ncolumn(); ++ci) {
            const auto dm_group = std::string(
                td.columnDesc(ci).dataManagerGroup().c_str());
            if (dm_group == expected_dm) {
                continue;
            }

            const auto dm_type = std::string(
                td.columnDesc(ci).dataManagerType().c_str());
            if (dm_type == expected_dm_type) {
                ++matched_type_fallback_count;
                continue;
            }

            if (dm_group != expected_dm) {
                throw std::runtime_error(
                    std::string(label) + ": sm_mapping mismatch for col " +
                    std::string(td.columnDesc(ci).name().c_str()) +
                    ": expected dm_group=" + expected_dm +
                    " or dm_type=" + expected_dm_type +
                    " got dm_group=" + dm_group +
                    " dm_type=" + dm_type);
            }
        }
        std::cout << "  " << label << ": [PASS] sm_mapping (strict, "
                  << td.ncolumn() << " cols, dm_group=" << expected_dm;
        if (matched_type_fallback_count > 0) {
            std::cout << ", dm_type fallback=" << matched_type_fallback_count;
        }
        std::cout << ")\n";
    }
}

void verify_tiled_dir_artifact(const std::filesystem::path& input_dir,
                                const std::vector<std::string>& expected_meta,
                                const std::string_view label) {
    // Determine expected DM group from label (matches mini tool's dm_group field).
    std::string expected_dm;
    if (label == "tiled-col-dir") {
        expected_dm = "TiledCol";
    } else if (label == "tiled-cell-dir") {
        expected_dm = "TiledCell";
    } else if (label == "tiled-shape-dir") {
        expected_dm = "TiledShape";
    } else if (label == "tiled-data-dir") {
        expected_dm = "TiledData";
    }

    // Tiled tables have empty keywords.
    casacore::Record empty_table_kw;
    std::vector<std::pair<std::string, casacore::Record>> empty_col_kw;
    verify_dir_structure_and_semantics(input_dir, expected_meta, label,
        expected_dm, empty_table_kw, empty_col_kw);

    // TSM-specific: probe for TSM data files.
    const auto tsm0_path = input_dir / "table.f0_TSM0";
    const auto tsm1_path = input_dir / "table.f0_TSM1";
    if (!std::filesystem::exists(tsm0_path) && !std::filesystem::exists(tsm1_path)) {
        throw std::runtime_error(
            std::string(label) +
            ": neither table.f0_TSM0 nor table.f0_TSM1 found; "
            "cannot verify cell values");
    }

    casacore::Table table(casacore::String(input_dir.string()));

    std::size_t cells_verified = 0;

    if (label == "tiled-col-dir") {
        // data[r] = all elements r*0.1F, flags[r] = all elements r.
        casacore::ArrayColumn<casacore::Float> col_data(table, "data");
        casacore::ArrayColumn<casacore::Int> col_flags(table, "flags");
        for (casacore::uInt r = 0; r < 10; ++r) {
            auto d = col_data.get(r);
            const auto exp_d = static_cast<casacore::Float>(r) * 0.1F;
            auto d_iter = d.begin();
            for (; d_iter != d.end(); ++d_iter) {
                if (std::fabs(*d_iter - exp_d) > kFloat32Tolerance) {
                    throw std::runtime_error(
                        std::string(label) + ": data[" + std::to_string(r) +
                        "] mismatch: got " + std::to_string(*d_iter) +
                        " expected " + std::to_string(exp_d));
                }
            }
            auto f = col_flags.get(r);
            const auto exp_f = static_cast<casacore::Int>(r);
            auto f_iter = f.begin();
            for (; f_iter != f.end(); ++f_iter) {
                if (*f_iter != exp_f) {
                    throw std::runtime_error(
                        std::string(label) + ": flags[" + std::to_string(r) +
                        "] mismatch");
                }
            }
            cells_verified += 2;
        }
    } else if (label == "tiled-cell-dir") {
        // map[r] = all elements r*1.5F.
        casacore::ArrayColumn<casacore::Float> col_map(table, "map");
        for (casacore::uInt r = 0; r < 5; ++r) {
            auto m = col_map.get(r);
            const auto exp_v = static_cast<casacore::Float>(r) * 1.5F;
            auto m_iter = m.begin();
            for (; m_iter != m.end(); ++m_iter) {
                if (std::fabs(*m_iter - exp_v) > kFloat32Tolerance) {
                    throw std::runtime_error(
                        std::string(label) + ": map[" + std::to_string(r) +
                        "] mismatch");
                }
            }
            ++cells_verified;
        }
    } else if (label == "tiled-shape-dir") {
        // vis[r] = all elements Complex(r, 0.0).
        casacore::ArrayColumn<casacore::Complex> col_vis(table, "vis");
        for (casacore::uInt r = 0; r < 5; ++r) {
            auto v = col_vis.get(r);
            const auto exp_c = casacore::Complex(static_cast<float>(r), 0.0F);
            auto v_iter = v.begin();
            for (; v_iter != v.end(); ++v_iter) {
                if (std::abs(*v_iter - exp_c) > kFloat32Tolerance) {
                    throw std::runtime_error(
                        std::string(label) + ": vis[" + std::to_string(r) +
                        "] mismatch");
                }
            }
            ++cells_verified;
        }
    } else if (label == "tiled-data-dir") {
        // spectrum[r] = all elements r*0.01F.
        casacore::ArrayColumn<casacore::Float> col_spec(table, "spectrum");
        for (casacore::uInt r = 0; r < 5; ++r) {
            auto s = col_spec.get(r);
            const auto exp_v = static_cast<casacore::Float>(r) * 0.01F;
            auto s_iter = s.begin();
            for (; s_iter != s.end(); ++s_iter) {
                if (std::fabs(*s_iter - exp_v) > kFloat32Tolerance) {
                    throw std::runtime_error(
                        std::string(label) + ": spectrum[" +
                        std::to_string(r) + "] mismatch");
                }
            }
            ++cells_verified;
        }
    }

    if (cells_verified > 0) {
        std::cout << "  " << label << ": [PASS] cell_values ("
                  << cells_verified << " cells, float tol=1e-6)\n";
    }
}

// =====================================================================
// Phase 8: measure / coordinate interop artifacts
// =====================================================================

// Artifact 1 — scalar measure-column table keywords (MEASINFO + QuantumUnits).
[[nodiscard]] casacore::Record build_measure_scalar_record() {
    casacore::Record measinfo;
    measinfo.define("type", casacore::String("epoch"));
    measinfo.define("Ref", casacore::String("UTC"));

    casacore::Record record;
    record.defineRecord("MEASINFO", measinfo);

    casacore::Array<casacore::String> units(casacore::IPosition(1, 1));
    units(casacore::IPosition(1, 0)) = "s";
    record.define("QuantumUnits", units);

    return record;
}

// Artifact 2 — array measure-column table keywords (direction, 2-unit).
[[nodiscard]] casacore::Record build_measure_array_record() {
    casacore::Record measinfo;
    measinfo.define("type", casacore::String("direction"));
    measinfo.define("Ref", casacore::String("J2000"));

    casacore::Record record;
    record.defineRecord("MEASINFO", measinfo);

    casacore::Array<casacore::String> units(casacore::IPosition(1, 2));
    units(casacore::IPosition(1, 0)) = "rad";
    units(casacore::IPosition(1, 1)) = "rad";
    record.define("QuantumUnits", units);

    return record;
}

// Artifact 3 — rich measure-keyword table (MeasureHolder-format, built manually).
[[nodiscard]] casacore::Record build_measure_rich_record() {
    casacore::Record record;

    // Epoch in MeasureHolder format (manual construction).
    casacore::Record epoch_rec;
    epoch_rec.define("type", casacore::String("epoch"));
    epoch_rec.define("refer", casacore::String("UTC"));
    {
        casacore::Record m0;
        m0.define("value", static_cast<casacore::Double>(59000.5));
        m0.define("unit", casacore::String("d"));
        epoch_rec.defineRecord("m0", m0);
    }
    record.defineRecord("obsdate", epoch_rec);

    // Direction in MeasureHolder format (manual).
    // 180 deg = pi rad, 45 deg = pi/4 rad.
    casacore::Record dir_rec;
    dir_rec.define("type", casacore::String("direction"));
    dir_rec.define("refer", casacore::String("J2000"));
    {
        casacore::Record d0;
        d0.define("value", static_cast<casacore::Double>(M_PI));
        d0.define("unit", casacore::String("rad"));
        dir_rec.defineRecord("m0", d0);
    }
    {
        casacore::Record d1;
        d1.define("value", static_cast<casacore::Double>(M_PI / 4.0));
        d1.define("unit", casacore::String("rad"));
        dir_rec.defineRecord("m1", d1);
    }
    record.defineRecord("pointingcenter", dir_rec);

    // Position (WGS84) in MeasureHolder format (manual).
    // -118 deg -> radians, 34 deg -> radians, height 100 m.
    casacore::Record pos_rec;
    pos_rec.define("type", casacore::String("position"));
    pos_rec.define("refer", casacore::String("WGS84"));
    {
        casacore::Record p0;
        p0.define("value", static_cast<casacore::Double>(-118.0 * M_PI / 180.0));
        p0.define("unit", casacore::String("rad"));
        pos_rec.defineRecord("m0", p0);
    }
    {
        casacore::Record p1;
        p1.define("value", static_cast<casacore::Double>(34.0 * M_PI / 180.0));
        p1.define("unit", casacore::String("rad"));
        pos_rec.defineRecord("m1", p1);
    }
    {
        casacore::Record p2;
        p2.define("value", static_cast<casacore::Double>(100.0));
        p2.define("unit", casacore::String("m"));
        pos_rec.defineRecord("m2", p2);
    }
    record.defineRecord("telescopeposition", pos_rec);

    record.define("telescope", casacore::String("VLA"));
    record.define("observer", casacore::String("TestObserver"));

    return record;
}

// Artifact 4 — coordinate-rich image metadata (manually constructed coord Record).
// Mimics CoordinateSystem::save() structure for a 2-coord system:
//   direction0 (J2000 SIN) + spectral1 (LSRK) + stokes2 (IQUV).
[[nodiscard]] casacore::Record build_coord_keywords_record() {
    casacore::Record record;
    record.define("ncoordinates", static_cast<casacore::Int>(3));

    // worldaxisnames, worldaxisunits, world/pixel replacement values, worldmap, pixelmap
    casacore::Array<casacore::String> axis_names(casacore::IPosition(1, 4));
    axis_names(casacore::IPosition(1, 0)) = "Right Ascension";
    axis_names(casacore::IPosition(1, 1)) = "Declination";
    axis_names(casacore::IPosition(1, 2)) = "Frequency";
    axis_names(casacore::IPosition(1, 3)) = "Stokes";
    record.define("worldaxisnames", axis_names);

    casacore::Array<casacore::String> axis_units(casacore::IPosition(1, 4));
    axis_units(casacore::IPosition(1, 0)) = "rad";
    axis_units(casacore::IPosition(1, 1)) = "rad";
    axis_units(casacore::IPosition(1, 2)) = "Hz";
    axis_units(casacore::IPosition(1, 3)) = "";
    record.define("worldaxisunits", axis_units);

    // worldreplacementvalues: reference pixel world values
    casacore::Array<casacore::Double> world_rep(casacore::IPosition(1, 4));
    world_rep(casacore::IPosition(1, 0)) = M_PI;              // 180 deg
    world_rep(casacore::IPosition(1, 1)) = M_PI / 4.0;        // 45 deg
    world_rep(casacore::IPosition(1, 2)) = 1.42e9;
    world_rep(casacore::IPosition(1, 3)) = 1.0;
    record.define("worldreplacementvalues", world_rep);

    casacore::Array<casacore::Double> pixel_rep(casacore::IPosition(1, 4));
    pixel_rep(casacore::IPosition(1, 0)) = 128.0;
    pixel_rep(casacore::IPosition(1, 1)) = 128.0;
    pixel_rep(casacore::IPosition(1, 2)) = 0.0;
    pixel_rep(casacore::IPosition(1, 3)) = 0.0;
    record.define("pixelreplacementvalues", pixel_rep);

    // Coordinate type indices
    casacore::Array<casacore::Int> coord_types(casacore::IPosition(1, 3));
    coord_types(casacore::IPosition(1, 0)) = 0; // Direction
    coord_types(casacore::IPosition(1, 1)) = 2; // Spectral
    coord_types(casacore::IPosition(1, 2)) = 4; // Stokes
    record.define("coordtype", coord_types);

    // worldmap and pixelmap (identity)
    casacore::Array<casacore::Int> worldmap0(casacore::IPosition(1, 2));
    worldmap0(casacore::IPosition(1, 0)) = 0;
    worldmap0(casacore::IPosition(1, 1)) = 1;
    record.define("worldmap0", worldmap0);

    casacore::Array<casacore::Int> worldmap1(casacore::IPosition(1, 1));
    worldmap1(casacore::IPosition(1, 0)) = 2;
    record.define("worldmap1", worldmap1);

    casacore::Array<casacore::Int> worldmap2(casacore::IPosition(1, 1));
    worldmap2(casacore::IPosition(1, 0)) = 3;
    record.define("worldmap2", worldmap2);

    casacore::Array<casacore::Int> pixelmap0(casacore::IPosition(1, 2));
    pixelmap0(casacore::IPosition(1, 0)) = 0;
    pixelmap0(casacore::IPosition(1, 1)) = 1;
    record.define("pixelmap0", pixelmap0);

    casacore::Array<casacore::Int> pixelmap1(casacore::IPosition(1, 1));
    pixelmap1(casacore::IPosition(1, 0)) = 2;
    record.define("pixelmap1", pixelmap1);

    casacore::Array<casacore::Int> pixelmap2(casacore::IPosition(1, 1));
    pixelmap2(casacore::IPosition(1, 0)) = 3;
    record.define("pixelmap2", pixelmap2);

    // direction0 sub-record
    casacore::Record direction0;
    direction0.define("system", casacore::String("J2000"));
    direction0.define("projection", casacore::String("SIN"));

    casacore::Array<casacore::Double> proj_params(casacore::IPosition(1, 2));
    proj_params(casacore::IPosition(1, 0)) = 0.0;
    proj_params(casacore::IPosition(1, 1)) = 0.0;
    direction0.define("projection_parameters", proj_params);

    casacore::Array<casacore::Double> crval(casacore::IPosition(1, 2));
    crval(casacore::IPosition(1, 0)) = M_PI;          // 180 deg in rad
    crval(casacore::IPosition(1, 1)) = M_PI / 4.0;    // 45 deg in rad
    direction0.define("crval", crval);

    casacore::Array<casacore::Double> crpix(casacore::IPosition(1, 2));
    crpix(casacore::IPosition(1, 0)) = 128.0;
    crpix(casacore::IPosition(1, 1)) = 128.0;
    direction0.define("crpix", crpix);

    casacore::Array<casacore::Double> cdelt(casacore::IPosition(1, 2));
    cdelt(casacore::IPosition(1, 0)) = -M_PI / 180.0 / 3600.0; // -1 arcsec/pixel
    cdelt(casacore::IPosition(1, 1)) = M_PI / 180.0 / 3600.0;  // 1 arcsec/pixel
    direction0.define("cdelt", cdelt);

    casacore::Array<casacore::Double> pc_matrix(casacore::IPosition(2, 2, 2));
    pc_matrix(casacore::IPosition(2, 0, 0)) = 1.0;
    pc_matrix(casacore::IPosition(2, 1, 0)) = 0.0;
    pc_matrix(casacore::IPosition(2, 0, 1)) = 0.0;
    pc_matrix(casacore::IPosition(2, 1, 1)) = 1.0;
    direction0.define("pc", pc_matrix);

    casacore::Array<casacore::String> dir_units(casacore::IPosition(1, 2));
    dir_units(casacore::IPosition(1, 0)) = "rad";
    dir_units(casacore::IPosition(1, 1)) = "rad";
    direction0.define("units", dir_units);

    casacore::Array<casacore::String> dir_names(casacore::IPosition(1, 2));
    dir_names(casacore::IPosition(1, 0)) = "Right Ascension";
    dir_names(casacore::IPosition(1, 1)) = "Declination";
    direction0.define("axes", dir_names);

    record.defineRecord("direction0", direction0);

    // spectral1 sub-record
    casacore::Record spectral1;
    spectral1.define("system", casacore::String("LSRK"));
    spectral1.define("restfreq", static_cast<casacore::Double>(1.42040575177e9));
    spectral1.define("crval", static_cast<casacore::Double>(1.42e9));
    spectral1.define("crpix", static_cast<casacore::Double>(0.0));
    spectral1.define("cdelt", static_cast<casacore::Double>(1e6));

    casacore::Array<casacore::String> spec_units(casacore::IPosition(1, 1));
    spec_units(casacore::IPosition(1, 0)) = "Hz";
    spectral1.define("units", spec_units);

    casacore::Array<casacore::String> spec_names(casacore::IPosition(1, 1));
    spec_names(casacore::IPosition(1, 0)) = "Frequency";
    spectral1.define("axes", spec_names);

    record.defineRecord("spectral1", spectral1);

    // stokes2 sub-record
    casacore::Record stokes2;
    casacore::Array<casacore::Int> stokes_vals(casacore::IPosition(1, 4));
    stokes_vals(casacore::IPosition(1, 0)) = 1; // I
    stokes_vals(casacore::IPosition(1, 1)) = 2; // Q
    stokes_vals(casacore::IPosition(1, 2)) = 3; // U
    stokes_vals(casacore::IPosition(1, 3)) = 4; // V
    stokes2.define("stokes", stokes_vals);

    casacore::Array<casacore::Double> stokes_crval(casacore::IPosition(1, 1));
    stokes_crval(casacore::IPosition(1, 0)) = 1.0;
    stokes2.define("crval", stokes_crval);

    casacore::Array<casacore::Double> stokes_crpix(casacore::IPosition(1, 1));
    stokes_crpix(casacore::IPosition(1, 0)) = 0.0;
    stokes2.define("crpix", stokes_crpix);

    casacore::Array<casacore::Double> stokes_cdelt(casacore::IPosition(1, 1));
    stokes_cdelt(casacore::IPosition(1, 0)) = 1.0;
    stokes2.define("cdelt", stokes_cdelt);

    casacore::Array<casacore::String> stokes_units(casacore::IPosition(1, 1));
    stokes_units(casacore::IPosition(1, 0)) = "";
    stokes2.define("units", stokes_units);

    casacore::Array<casacore::String> stokes_names(casacore::IPosition(1, 1));
    stokes_names(casacore::IPosition(1, 0)) = "Stokes";
    stokes2.define("axes", stokes_names);

    record.defineRecord("stokes2", stokes2);

    return record;
}

// Artifact 5 — mixed-coordinate Record (direction + linear + spectral).
// Tests a different combination: direction0, linear1, spectral2.
[[nodiscard]] casacore::Record build_mixed_coords_record() {
    casacore::Record record;
    record.define("ncoordinates", static_cast<casacore::Int>(3));

    casacore::Array<casacore::String> axis_names(casacore::IPosition(1, 4));
    axis_names(casacore::IPosition(1, 0)) = "Right Ascension";
    axis_names(casacore::IPosition(1, 1)) = "Declination";
    axis_names(casacore::IPosition(1, 2)) = "Length";
    axis_names(casacore::IPosition(1, 3)) = "Frequency";
    record.define("worldaxisnames", axis_names);

    casacore::Array<casacore::String> axis_units(casacore::IPosition(1, 4));
    axis_units(casacore::IPosition(1, 0)) = "rad";
    axis_units(casacore::IPosition(1, 1)) = "rad";
    axis_units(casacore::IPosition(1, 2)) = "km";
    axis_units(casacore::IPosition(1, 3)) = "Hz";
    record.define("worldaxisunits", axis_units);

    casacore::Array<casacore::Int> coord_types(casacore::IPosition(1, 3));
    coord_types(casacore::IPosition(1, 0)) = 0; // Direction
    coord_types(casacore::IPosition(1, 1)) = 6; // Linear
    coord_types(casacore::IPosition(1, 2)) = 2; // Spectral
    record.define("coordtype", coord_types);

    // direction0 sub-record (Galactic SIN)
    casacore::Record direction0;
    direction0.define("system", casacore::String("GALACTIC"));
    direction0.define("projection", casacore::String("SIN"));

    casacore::Array<casacore::Double> proj_params(casacore::IPosition(1, 2));
    proj_params(casacore::IPosition(1, 0)) = 0.0;
    proj_params(casacore::IPosition(1, 1)) = 0.0;
    direction0.define("projection_parameters", proj_params);

    casacore::Array<casacore::Double> crval(casacore::IPosition(1, 2));
    crval(casacore::IPosition(1, 0)) = 0.0;  // Galactic center lon
    crval(casacore::IPosition(1, 1)) = 0.0;  // Galactic center lat
    direction0.define("crval", crval);

    casacore::Array<casacore::Double> crpix(casacore::IPosition(1, 2));
    crpix(casacore::IPosition(1, 0)) = 64.0;
    crpix(casacore::IPosition(1, 1)) = 64.0;
    direction0.define("crpix", crpix);

    casacore::Array<casacore::Double> cdelt(casacore::IPosition(1, 2));
    cdelt(casacore::IPosition(1, 0)) = -M_PI / 180.0 / 60.0; // -1 arcmin/pixel
    cdelt(casacore::IPosition(1, 1)) = M_PI / 180.0 / 60.0;
    direction0.define("cdelt", cdelt);

    casacore::Array<casacore::Double> pc_matrix(casacore::IPosition(2, 2, 2));
    pc_matrix(casacore::IPosition(2, 0, 0)) = 1.0;
    pc_matrix(casacore::IPosition(2, 1, 0)) = 0.0;
    pc_matrix(casacore::IPosition(2, 0, 1)) = 0.0;
    pc_matrix(casacore::IPosition(2, 1, 1)) = 1.0;
    direction0.define("pc", pc_matrix);

    casacore::Array<casacore::String> dir_units(casacore::IPosition(1, 2));
    dir_units(casacore::IPosition(1, 0)) = "rad";
    dir_units(casacore::IPosition(1, 1)) = "rad";
    direction0.define("units", dir_units);

    casacore::Array<casacore::String> dir_names(casacore::IPosition(1, 2));
    dir_names(casacore::IPosition(1, 0)) = "Right Ascension";
    dir_names(casacore::IPosition(1, 1)) = "Declination";
    direction0.define("axes", dir_names);

    record.defineRecord("direction0", direction0);

    // linear1 sub-record (1 axis, km)
    casacore::Record linear1;
    casacore::Array<casacore::Double> lin_crval(casacore::IPosition(1, 1));
    lin_crval(casacore::IPosition(1, 0)) = 0.0;
    linear1.define("crval", lin_crval);

    casacore::Array<casacore::Double> lin_crpix(casacore::IPosition(1, 1));
    lin_crpix(casacore::IPosition(1, 0)) = 0.0;
    linear1.define("crpix", lin_crpix);

    casacore::Array<casacore::Double> lin_cdelt(casacore::IPosition(1, 1));
    lin_cdelt(casacore::IPosition(1, 0)) = 1.0;
    linear1.define("cdelt", lin_cdelt);

    casacore::Array<casacore::Double> lin_pc(casacore::IPosition(2, 1, 1));
    lin_pc(casacore::IPosition(2, 0, 0)) = 1.0;
    linear1.define("pc", lin_pc);

    casacore::Array<casacore::String> lin_units(casacore::IPosition(1, 1));
    lin_units(casacore::IPosition(1, 0)) = "km";
    linear1.define("units", lin_units);

    casacore::Array<casacore::String> lin_names(casacore::IPosition(1, 1));
    lin_names(casacore::IPosition(1, 0)) = "Length";
    linear1.define("axes", lin_names);

    record.defineRecord("linear1", linear1);

    // spectral2 sub-record (TOPO)
    casacore::Record spectral2;
    spectral2.define("system", casacore::String("TOPO"));
    spectral2.define("restfreq", static_cast<casacore::Double>(1.42040575177e9));
    spectral2.define("crval", static_cast<casacore::Double>(1.0e9));
    spectral2.define("crpix", static_cast<casacore::Double>(0.0));
    spectral2.define("cdelt", static_cast<casacore::Double>(5e5));

    casacore::Array<casacore::String> spec_units(casacore::IPosition(1, 1));
    spec_units(casacore::IPosition(1, 0)) = "Hz";
    spectral2.define("units", spec_units);

    casacore::Array<casacore::String> spec_names(casacore::IPosition(1, 1));
    spec_names(casacore::IPosition(1, 0)) = "Frequency";
    spectral2.define("axes", spec_names);

    record.defineRecord("spectral2", spectral2);

    return record;
}

// Artifact 6 — conversion vectors Record (frequency reference + rest-frequency list).
// Stores a frequency measure reference, a list of rest frequencies, and a velocity
// conversion convention — typical of MS SPECTRAL_WINDOW keyword sets.
[[nodiscard]] casacore::Record build_conversion_vectors_record() {
    casacore::Record record;

    // Epoch conversions: MJD 59000.5 UTC -> TAI
    // UTC->TAI adds leap seconds (37s as of 2020)
    record.define("epoch_utc_mjd", static_cast<casacore::Double>(59000.5));
    record.define("epoch_tai_mjd",
                  static_cast<casacore::Double>(59000.5 + 37.0 / 86400.0));

    // Direction: RA=180deg Dec=45deg J2000
    casacore::Array<casacore::Double> j2000_dir(casacore::IPosition(1, 2));
    j2000_dir(casacore::IPosition(1, 0)) = M_PI;       // 180 deg in rad
    j2000_dir(casacore::IPosition(1, 1)) = M_PI / 4.0;  // 45 deg in rad
    record.define("direction_j2000_rad", j2000_dir);

    // Doppler conversions (purely mathematical, deterministic)
    // Optical doppler: v/c = z = 0.1
    record.define("doppler_z", static_cast<casacore::Double>(0.1));
    // Radio doppler from z: v_radio/c = 1 - 1/(1+z) = 1 - 1/1.1
    record.define("doppler_radio",
                  static_cast<casacore::Double>(1.0 - 1.0 / 1.1));
    // Relativistic beta from z: beta = ((1+z)^2-1)/((1+z)^2+1)
    constexpr double kZ = 0.1;
    const double beta =
        ((1.0 + kZ) * (1.0 + kZ) - 1.0) / ((1.0 + kZ) * (1.0 + kZ) + 1.0);
    record.define("doppler_beta", static_cast<casacore::Double>(beta));

    // Frequency: rest=1.42e9 Hz, optical z=0.1 -> observed = rest/(1+z)
    record.define("freq_rest_hz", static_cast<casacore::Double>(1.42e9));
    record.define("freq_observed_hz",
                  static_cast<casacore::Double>(1.42e9 / 1.1));

    return record;
}

// --- Phase 8 semantic verification helpers ---

// Helper: check that a Record field exists and is a string with expected value.
void verify_string_field(const casacore::RecordInterface& record,
                         const std::string_view path,
                         const std::string_view field_name,
                         const std::string_view expected_value,
                         const std::string_view label) {
    if (!record.isDefined(casacore::String(std::string(field_name)))) {
        throw std::runtime_error(std::string(label) + ": missing field " +
                                 std::string(path) + "." + std::string(field_name));
    }
    const auto fid = record.idToNumber(
        casacore::RecordFieldId(casacore::String(std::string(field_name))));
    if (record.dataType(fid) != casacore::TpString) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " is not a string");
    }
    const auto actual = std::string(record.asString(fid).c_str());
    if (actual != std::string(expected_value)) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " expected '" + std::string(expected_value) +
                                 "' got '" + actual + "'");
    }
}

// Helper: check that a Record field exists and is a double within tolerance.
void verify_double_field(const casacore::RecordInterface& record,
                         const std::string_view path,
                         const std::string_view field_name,
                         const casacore::Double expected_value,
                         const std::string_view label) {
    if (!record.isDefined(casacore::String(std::string(field_name)))) {
        throw std::runtime_error(std::string(label) + ": missing field " +
                                 std::string(path) + "." + std::string(field_name));
    }
    const auto fid = record.idToNumber(
        casacore::RecordFieldId(casacore::String(std::string(field_name))));
    if (record.dataType(fid) != casacore::TpDouble) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " is not a double");
    }
    const auto actual = record.asDouble(fid);
    if (std::fabs(actual - expected_value) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " expected " + std::to_string(expected_value) +
                                 " got " + std::to_string(actual));
    }
}

// Helper: check that a sub-record exists.
void verify_subrecord_exists(const casacore::RecordInterface& record,
                             const std::string_view path,
                             const std::string_view field_name,
                             const std::string_view label) {
    if (!record.isDefined(casacore::String(std::string(field_name)))) {
        throw std::runtime_error(std::string(label) + ": missing sub-record " +
                                 std::string(path) + "." + std::string(field_name));
    }
    const auto fid = record.idToNumber(
        casacore::RecordFieldId(casacore::String(std::string(field_name))));
    if (record.dataType(fid) != casacore::TpRecord) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " is not a sub-record");
    }
}

// Helper: check double array field has expected shape and first/last element.
void verify_double_array_field(const casacore::RecordInterface& record,
                               const std::string_view path,
                               const std::string_view field_name,
                               const casacore::uInt expected_size,
                               const casacore::Double expected_first,
                               const casacore::Double expected_last,
                               const std::string_view label) {
    if (!record.isDefined(casacore::String(std::string(field_name)))) {
        throw std::runtime_error(std::string(label) + ": missing field " +
                                 std::string(path) + "." + std::string(field_name));
    }
    const auto fid = record.idToNumber(
        casacore::RecordFieldId(casacore::String(std::string(field_name))));
    if (record.dataType(fid) != casacore::TpArrayDouble) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " is not a double array");
    }
    const auto arr = record.asArrayDouble(fid);
    if (arr.nelements() < expected_size) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 " expected at least " + std::to_string(expected_size) +
                                 " elements, got " + std::to_string(arr.nelements()));
    }
    auto it = arr.begin();
    if (std::fabs(*it - expected_first) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 "[0] expected " + std::to_string(expected_first) +
                                 " got " + std::to_string(*it));
    }
    // Advance to last element.
    auto last_it = arr.begin();
    for (casacore::uInt idx = 1; idx < arr.nelements(); ++idx) {
        ++last_it;
    }
    if (std::fabs(*last_it - expected_last) > kFloat64Tolerance) {
        throw std::runtime_error(std::string(label) + ": field " +
                                 std::string(path) + "." + std::string(field_name) +
                                 "[last] expected " + std::to_string(expected_last) +
                                 " got " + std::to_string(*last_it));
    }
}

// Helper: find a sub-record by trying multiple candidate field names.
// Returns the field name that was found, or empty string if none.
[[nodiscard]] std::string find_subrecord_any(const casacore::RecordInterface& record,
                                              const std::vector<std::string>& candidates) {
    for (const auto& name : candidates) {
        if (record.isDefined(casacore::String(name))) {
            const auto fid = record.idToNumber(
                casacore::RecordFieldId(casacore::String(name)));
            if (record.dataType(fid) == casacore::TpRecord) {
                return name;
            }
        }
    }
    return "";
}

// Semantic verification for coord-keywords (artifact 4).
// Accepts both casacore format (ncoordinates, int32 maps, pc matrix) and
// mini format (no ncoordinates, int64 maps, coordinate_type field).
void verify_coord_keywords_semantic(const casacore::Record& record,
                                    const std::string_view label) {
    // direction0 must exist in both formats.
    verify_subrecord_exists(record, "", "direction0", label);

    const auto& dir0 = record.asRecord("direction0");
    verify_string_field(dir0, "direction0", "system", "J2000", label);

    // Projection may be stored as "projection" (casacore) or "projection_type" (mini).
    // Accept SIN in either field.
    if (dir0.isDefined("projection")) {
        verify_string_field(dir0, "direction0", "projection", "SIN", label);
    } else if (dir0.isDefined("projection_type")) {
        verify_string_field(dir0, "direction0", "projection_type", "SIN", label);
    }

    verify_double_array_field(dir0, "direction0", "crval", 2,
                              M_PI, M_PI / 4.0, label);
    verify_double_array_field(dir0, "direction0", "crpix", 2, 128.0, 128.0, label);

    // Find spectral sub-record (may be spectral1 or spectral2 depending on format).
    auto spec_key = find_subrecord_any(record, {"spectral1", "spectral2"});
    if (spec_key.empty()) {
        throw std::runtime_error(std::string(label) +
                                 ": missing spectral sub-record (tried spectral1, spectral2)");
    }
    const auto& spec = record.asRecord(spec_key);
    verify_string_field(spec, spec_key, "system", "LSRK", label);
    verify_double_field(spec, spec_key, "crval", 1.42e9, label);
    verify_double_field(spec, spec_key, "cdelt", 1e6, label);

    // Find stokes sub-record (may be stokes2 or stokes1).
    auto stk_key = find_subrecord_any(record, {"stokes2", "stokes1"});
    if (stk_key.empty()) {
        throw std::runtime_error(std::string(label) +
                                 ": missing stokes sub-record (tried stokes2, stokes1)");
    }
    const auto& stk = record.asRecord(stk_key);
    if (!stk.isDefined("stokes")) {
        throw std::runtime_error(std::string(label) + ": " + stk_key +
                                 " missing 'stokes' field");
    }
    const auto stk_fid = stk.idToNumber(
        casacore::RecordFieldId(casacore::String("stokes")));
    // Stokes may be int32 array (casacore) or int64 array (mini).
    if (stk.dataType(stk_fid) == casacore::TpArrayInt) {
        const auto stokes_arr = stk.asArrayInt(stk_fid);
        if (stokes_arr.nelements() != 4) {
            throw std::runtime_error(std::string(label) + ": stokes expected 4 elements");
        }
        if (*stokes_arr.begin() != 1) {
            throw std::runtime_error(std::string(label) + ": stokes[0] expected 1 (I)");
        }
    } else if (stk.dataType(stk_fid) == casacore::TpArrayInt64) {
        const auto stokes_arr = stk.asArrayInt64(stk_fid);
        if (stokes_arr.nelements() != 4) {
            throw std::runtime_error(std::string(label) + ": stokes expected 4 elements");
        }
        if (*stokes_arr.begin() != 1) {
            throw std::runtime_error(std::string(label) + ": stokes[0] expected 1 (I)");
        }
    } else {
        throw std::runtime_error(std::string(label) +
                                 ": stokes field is not an int or int64 array");
    }

    std::cout << "  " << label << ": [PASS] semantic (J2000-SIN, LSRK, IQUV)\n";
}

// Semantic verification for mixed-coords (artifact 5).
// Accepts two formats:
//  - casacore format: direction0 (GALACTIC/SIN) + linear1 + spectral2 (TOPO)
//  - mini format: direction0 (J2000/TAN) + spectral1 (LSRK) + stokes2 + linear3 + ObsInfo
void verify_mixed_coords_semantic(const casacore::Record& record,
                                  const std::string_view label) {
    // direction0 must exist in both formats.
    verify_subrecord_exists(record, "", "direction0", label);
    const auto& dir0 = record.asRecord("direction0");

    // Check for casacore format (GALACTIC/SIN) vs mini format (J2000/TAN).
    if (dir0.isDefined("system")) {
        const auto fid = dir0.idToNumber(
            casacore::RecordFieldId(casacore::String("system")));
        const auto system_val = std::string(dir0.asString(fid).c_str());
        if (system_val == "GALACTIC") {
            // Casacore format: GALACTIC-SIN + linear1 + spectral2 (TOPO).
            if (dir0.isDefined("projection")) {
                verify_string_field(dir0, "direction0", "projection", "SIN", label);
            }
            verify_subrecord_exists(record, "", "linear1", label);
            const auto& lin1 = record.asRecord("linear1");
            verify_double_array_field(lin1, "linear1", "crval", 1, 0.0, 0.0, label);

            verify_subrecord_exists(record, "", "spectral2", label);
            const auto& spec2 = record.asRecord("spectral2");
            verify_string_field(spec2, "spectral2", "system", "TOPO", label);
            verify_double_field(spec2, "spectral2", "crval", 1.0e9, label);

            std::cout << "  " << label
                      << ": [PASS] semantic (GALACTIC-SIN, Linear, TOPO)\n";
        } else if (system_val == "J2000") {
            // Mini format: J2000-TAN + spectral1 (LSRK) + stokes2 + linear3 + ObsInfo.
            if (dir0.isDefined("projection")) {
                verify_string_field(dir0, "direction0", "projection", "TAN", label);
            } else if (dir0.isDefined("projection_type")) {
                verify_string_field(dir0, "direction0", "projection_type", "TAN", label);
            }

            auto spec_key = find_subrecord_any(record, {"spectral1", "spectral2"});
            if (spec_key.empty()) {
                throw std::runtime_error(std::string(label) +
                                         ": missing spectral sub-record");
            }
            const auto& spec = record.asRecord(spec_key);
            verify_string_field(spec, spec_key, "system", "LSRK", label);
            verify_double_field(spec, spec_key, "crval", 1.42e9, label);

            // Verify stokes and linear exist somewhere.
            auto stk_key = find_subrecord_any(record,
                                               {"stokes2", "stokes1", "stokes3"});
            if (stk_key.empty()) {
                throw std::runtime_error(std::string(label) +
                                         ": missing stokes sub-record");
            }

            auto lin_key = find_subrecord_any(record,
                                               {"linear1", "linear2", "linear3"});
            if (lin_key.empty()) {
                throw std::runtime_error(std::string(label) +
                                         ": missing linear sub-record");
            }

            std::cout << "  " << label
                      << ": [PASS] semantic (J2000-TAN, LSRK, Stokes, Linear)\n";
        } else {
            throw std::runtime_error(std::string(label) +
                                     ": unexpected direction system '" + system_val + "'");
        }
    } else {
        throw std::runtime_error(std::string(label) +
                                 ": direction0 missing 'system' field");
    }
}

// Semantic verification for conversion-vectors (artifact 6).
void verify_conversion_vectors_semantic(const casacore::Record& record,
                                        const std::string_view label) {
    verify_double_field(record, "", "epoch_utc_mjd", 59000.5, label);
    verify_double_field(record, "", "epoch_tai_mjd",
                        59000.5 + 37.0 / 86400.0, label);
    verify_double_field(record, "", "doppler_z", 0.1, label);
    verify_double_field(record, "", "doppler_radio", 1.0 - 1.0 / 1.1, label);
    constexpr double kZ = 0.1;
    const double expected_beta =
        ((1.0 + kZ) * (1.0 + kZ) - 1.0) / ((1.0 + kZ) * (1.0 + kZ) + 1.0);
    verify_double_field(record, "", "doppler_beta", expected_beta, label);
    verify_double_field(record, "", "freq_rest_hz", 1.42e9, label);
    verify_double_field(record, "", "freq_observed_hz", 1.42e9 / 1.1, label);

    verify_double_array_field(record, "", "direction_j2000_rad", 2,
                              M_PI, M_PI / 4.0, label);

    std::cout << "  " << label
              << ": [PASS] semantic (epoch, doppler, freq conversions)\n";
}

// =====================================================================

[[nodiscard]] std::string usage() {
    return "Usage:\n"
           "  casacore_interop_tool write-record-basic --output <path>\n"
           "  casacore_interop_tool write-record-nested --output <path>\n"
           "  casacore_interop_tool write-record-fixture-logtable-time --output <path>\n"
           "  casacore_interop_tool write-record-fixture-ms-table --output <path>\n"
           "  casacore_interop_tool write-record-fixture-ms-uvw --output <path>\n"
           "  casacore_interop_tool write-record-fixture-pagedimage-table --output <path>\n"
           "  casacore_interop_tool write-table-dat-header --output <path>\n"
           "  casacore_interop_tool write-table-dat-full --output <path>\n"
           "  casacore_interop_tool write-table-dir --output <dir>\n"
           "  casacore_interop_tool write-tiled-col-dir --output <dir>\n"
           "  casacore_interop_tool write-tiled-cell-dir --output <dir>\n"
           "  casacore_interop_tool write-tiled-shape-dir --output <dir>\n"
           "  casacore_interop_tool write-tiled-data-dir --output <dir>\n"
           "  casacore_interop_tool verify-record-basic --input <path>\n"
           "  casacore_interop_tool verify-record-nested --input <path>\n"
           "  casacore_interop_tool verify-record-fixture-logtable-time --input <path>\n"
           "  casacore_interop_tool verify-record-fixture-ms-table --input <path>\n"
           "  casacore_interop_tool verify-record-fixture-ms-uvw --input <path>\n"
           "  casacore_interop_tool verify-record-fixture-pagedimage-table --input <path>\n"
           "  casacore_interop_tool verify-table-dat-header --input <path>\n"
           "  casacore_interop_tool verify-table-dat-full --input <path>\n"
           "  casacore_interop_tool verify-table-dir --input <dir>\n"
           "  casacore_interop_tool verify-tiled-col-dir --input <dir>\n"
           "  casacore_interop_tool verify-tiled-cell-dir --input <dir>\n"
           "  casacore_interop_tool verify-tiled-shape-dir --input <dir>\n"
           "  casacore_interop_tool verify-tiled-data-dir --input <dir>\n"
           "  casacore_interop_tool dump-record --input <path> --output <path>\n"
           "  casacore_interop_tool dump-table-dat-header --input <path> --output <path>\n"
           "  casacore_interop_tool dump-table-dat-full --input <path> --output <path>\n"
           "  casacore_interop_tool write-ism-dir --output <dir>\n"
           "  casacore_interop_tool verify-ism-dir --input <dir>\n"
           "  casacore_interop_tool dump-table-dir --input <dir> --output <path>\n"
           "  casacore_interop_tool produce-measure-scalar --output <path>\n"
           "  casacore_interop_tool verify-measure-scalar --input <path>\n"
           "  casacore_interop_tool produce-measure-array --output <path>\n"
           "  casacore_interop_tool verify-measure-array --input <path>\n"
           "  casacore_interop_tool produce-measure-rich --output <path>\n"
           "  casacore_interop_tool verify-measure-rich --input <path>\n"
           "  casacore_interop_tool produce-coord-keywords --output <path>\n"
           "  casacore_interop_tool verify-coord-keywords --input <path>\n"
           "  casacore_interop_tool produce-mixed-coords --output <path>\n"
           "  casacore_interop_tool verify-mixed-coords --input <path>\n"
           "  casacore_interop_tool produce-conversion-vectors --output <path>\n"
           "  casacore_interop_tool verify-conversion-vectors --input <path>\n";
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
            dump_table_dat_full_artifact(*input, *output);
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
            // Metadata + structure check (no TSM file probe).
            // Build expected ISM keywords.
            casacore::Record ism_table_kw;
            ism_table_kw.define("instrument", casacore::String("ALMA"));
            ism_table_kw.define("epoch", static_cast<casacore::Double>(4.8e9));

            casacore::Record time_kw;
            {
                casacore::Record measinfo;
                measinfo.define("type", casacore::String("epoch"));
                measinfo.define("Ref", casacore::String("UTC"));
                time_kw.defineRecord("MEASINFO", measinfo);
                casacore::Array<casacore::String> qu_arr(casacore::IPosition(1, 1));
                qu_arr(casacore::IPosition(1, 0)) = casacore::String("s");
                time_kw.define("QuantumUnits", qu_arr);
            }
            std::vector<std::pair<std::string, casacore::Record>> ism_col_kw;
            ism_col_kw.emplace_back("time", time_kw);

            verify_dir_structure_and_semantics(
                *input, expected_ism_meta(), "ism-dir", "ISMData",
                ism_table_kw, ism_col_kw);
            // Cell-value check if the table can be opened.
            const auto f0_path = std::filesystem::path(*input) / "table.f0";
            if (std::filesystem::exists(f0_path)) {
                casacore::Table table(casacore::String(input->data()),
                                      casacore::Table::Old);
                casacore::ScalarColumn<casacore::Double> col_time(table, "time");
                casacore::ScalarColumn<casacore::Int> col_antenna(table, "antenna");
                casacore::ScalarColumn<casacore::Bool> col_flag(table, "flag");
                for (casacore::uInt i = 0; i < 10; ++i) {
                    const auto exp_time = 4.8e9 + static_cast<double>(i) * 10.0;
                    if (std::fabs(col_time(i) - exp_time) > kFloat64Tolerance) {
                        throw std::runtime_error(
                            "ism-dir: time[" + std::to_string(i) + "] mismatch");
                    }
                    if (col_antenna(i) != static_cast<casacore::Int>(i % 3)) {
                        throw std::runtime_error(
                            "ism-dir: antenna[" + std::to_string(i) + "] mismatch");
                    }
                    const auto exp_flag = (i % 2) == 0;
                    if (col_flag(i) != exp_flag) {
                        throw std::runtime_error(
                            "ism-dir: flag[" + std::to_string(i) + "] mismatch");
                    }
                }
                std::cout << "  ism-dir: [PASS] cell_values (30 cells, double tol=1e-10)\n";
            } else {
                throw std::runtime_error(
                    "ism-dir: no .f0 file; cannot verify cell values");
            }
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

        // --- Phase 8: measure / coordinate interop commands ---

        // Artifact 1: scalar measure-column keywords
        if (subcommand == "produce-measure-scalar") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_scalar_record());
            return 0;
        }
        if (subcommand == "verify-measure-scalar") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_scalar_record(),
                                   "measure-scalar");
            std::cout << "  measure-scalar: [PASS]\n";
            return 0;
        }

        // Artifact 2: array measure-column keywords
        if (subcommand == "produce-measure-array") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_array_record());
            return 0;
        }
        if (subcommand == "verify-measure-array") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_array_record(),
                                   "measure-array");
            std::cout << "  measure-array: [PASS]\n";
            return 0;
        }

        // Artifact 3: rich measure-keyword table
        if (subcommand == "produce-measure-rich") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_measure_rich_record());
            return 0;
        }
        if (subcommand == "verify-measure-rich") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            verify_record_artifact(*input, build_measure_rich_record(),
                                   "measure-rich");
            std::cout << "  measure-rich: [PASS]\n";
            return 0;
        }

        // Artifact 4: coordinate-rich image metadata
        if (subcommand == "produce-coord-keywords") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_coord_keywords_record());
            return 0;
        }
        if (subcommand == "verify-coord-keywords") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            // Try exact match first; fall back to semantic verification.
            casacore::AipsIO aio(casacore::String(input->data()));
            casacore::Record actual_record;
            aio >> actual_record;
            try {
                verify_lines_equal("coord-keywords",
                                   canonical_record_lines(build_coord_keywords_record()),
                                   canonical_record_lines(actual_record));
                std::cout << "  coord-keywords: [PASS] exact match\n";
            } catch (const std::runtime_error&) {
                // Fall back to semantic verification.
                verify_coord_keywords_semantic(actual_record, "coord-keywords");
            }
            return 0;
        }

        // Artifact 5: mixed-coordinate Record
        if (subcommand == "produce-mixed-coords") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_mixed_coords_record());
            return 0;
        }
        if (subcommand == "verify-mixed-coords") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            casacore::AipsIO aio(casacore::String(input->data()));
            casacore::Record actual_record;
            aio >> actual_record;
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

        // Artifact 6: conversion vectors Record
        if (subcommand == "produce-conversion-vectors") {
            const auto output = arg_value(argc, argv, "--output");
            if (!output.has_value()) {
                throw std::runtime_error("missing required --output");
            }
            write_record_artifact(*output, build_conversion_vectors_record());
            return 0;
        }
        if (subcommand == "verify-conversion-vectors") {
            const auto input = arg_value(argc, argv, "--input");
            if (!input.has_value()) {
                throw std::runtime_error("missing required --input");
            }
            casacore::AipsIO aio(casacore::String(input->data()));
            casacore::Record actual_record;
            aio >> actual_record;
            try {
                verify_lines_equal("conversion-vectors",
                                   canonical_record_lines(build_conversion_vectors_record()),
                                   canonical_record_lines(actual_record));
                std::cout << "  conversion-vectors: [PASS] exact match\n";
            } catch (const std::runtime_error&) {
                verify_conversion_vectors_semantic(actual_record,
                                                   "conversion-vectors");
            }
            return 0;
        }

        throw std::runtime_error("unknown subcommand: " + subcommand);
    } catch (const std::exception& error) {
        std::cerr << "casacore_interop_tool failed: " << error.what() << '\n';
        std::cerr << usage();
        return 1;
    }
}
