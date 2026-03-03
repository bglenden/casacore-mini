// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

#include <complex>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect_true(const bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

/// Helper: build an AipsIO byte stream encoding a Record the way casacore does.
/// This manually constructs the wire format using the AipsIoWriter primitives.
class RecordBuilder {
  public:
    /// Start a Record object. Call end_record() when done.
    void begin_record(const std::vector<std::pair<std::string, std::int32_t>>& fields) {
        // Top-level Record object header includes the AipsIO magic marker.
        writer_.write_object_header(0U, "Record", 1U);
        write_record_desc(fields);
        writer_.write_i32(1); // recordType = Variable
    }

    /// Write a RecordDesc with only scalar fields (no sub-records or arrays in desc).
    void write_record_desc(const std::vector<std::pair<std::string, std::int32_t>>& fields) {
        write_embedded_object_header("RecordDesc", 2U);
        writer_.write_i32(static_cast<std::int32_t>(fields.size()));
        for (const auto& [name, type] : fields) {
            writer_.write_string(name);
            writer_.write_i32(type);
            // For array types, write an empty IPosition shape.
            if ((type >= 13 && type <= 24) || type == 30) {
                write_iposition({});
            }
            writer_.write_string(""); // comment (version 2)
        }
    }

    void write_iposition(const std::vector<std::uint32_t>& dims) {
        write_embedded_object_header("IPosition", 1U);
        writer_.write_u32(static_cast<std::uint32_t>(dims.size()));
        for (const auto dim : dims) {
            writer_.write_i32(static_cast<std::int32_t>(dim));
        }
    }

    void write_bool(bool value) {
        writer_.write_u8(value ? 1U : 0U);
    }
    void write_i32(std::int32_t value) {
        writer_.write_i32(value);
    }
    void write_u32(std::uint32_t value) {
        writer_.write_u32(value);
    }
    void write_i64(std::int64_t value) {
        writer_.write_i64(value);
    }
    void write_f32(float value) {
        writer_.write_f32(value);
    }
    void write_f64(double value) {
        writer_.write_f64(value);
    }
    void write_complex64(std::complex<float> value) {
        writer_.write_complex64(value);
    }
    void write_complex128(std::complex<double> value) {
        writer_.write_complex128(value);
    }
    void write_string(const std::string& value) {
        writer_.write_string(value);
    }

    /// Write an Array<T> header (version 3 format).
    void begin_array(const std::string& type_name, const std::vector<std::uint32_t>& shape,
                     std::uint32_t nelements) {
        write_embedded_object_header(type_name, 3U);
        writer_.write_u32(static_cast<std::uint32_t>(shape.size()));
        for (const auto dim : shape) {
            writer_.write_u32(dim);
        }
        writer_.write_u32(nelements);
    }

    [[nodiscard]] std::vector<std::uint8_t> take_bytes() {
        return writer_.take_bytes();
    }

  private:
    void write_embedded_object_header(const std::string& type_name, const std::uint32_t version) {
        // Embedded objects omit magic in casacore streams.
        writer_.write_u32(0U); // placeholder length
        writer_.write_string(type_name);
        writer_.write_u32(version);
    }

    casacore_mini::AipsIoWriter writer_;
};

bool test_scalar_record() {
    // DataType enum values from casacore.
    constexpr std::int32_t kTpBool = 0;
    constexpr std::int32_t kTpInt = 5;
    constexpr std::int32_t kTpFloat = 7;
    constexpr std::int32_t kTpDouble = 8;
    constexpr std::int32_t kTpString = 11;
    constexpr std::int32_t kTpInt64 = 29;

    RecordBuilder builder;
    builder.begin_record({
        {"flag", kTpBool},
        {"count", kTpInt},
        {"rate", kTpFloat},
        {"value", kTpDouble},
        {"label", kTpString},
        {"big", kTpInt64},
    });
    builder.write_bool(true);
    builder.write_i32(42);
    builder.write_f32(1.5F);
    builder.write_f64(3.14);
    builder.write_string("hello");
    builder.write_i64(9000000000LL);

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    if (!expect_true(record.size() == 6U, "scalar record should have 6 fields")) {
        return false;
    }

    const auto* flag = record.find("flag");
    if (!expect_true(flag != nullptr && std::holds_alternative<bool>(flag->storage()),
                     "flag should be bool")) {
        return false;
    }
    if (!expect_true(std::get<bool>(flag->storage()) == true, "flag should be true")) {
        return false;
    }

    const auto* count = record.find("count");
    if (!expect_true(count != nullptr && std::holds_alternative<std::int32_t>(count->storage()),
                     "count should be i32")) {
        return false;
    }
    if (!expect_true(std::get<std::int32_t>(count->storage()) == 42, "count should be 42")) {
        return false;
    }

    const auto* rate = record.find("rate");
    if (!expect_true(rate != nullptr && std::holds_alternative<float>(rate->storage()),
                     "rate should be float")) {
        return false;
    }
    if (!expect_true(std::get<float>(rate->storage()) == 1.5F, "rate should be 1.5")) {
        return false;
    }

    const auto* value = record.find("value");
    if (!expect_true(value != nullptr && std::holds_alternative<double>(value->storage()),
                     "value should be double")) {
        return false;
    }
    if (!expect_true(std::get<double>(value->storage()) == 3.14, "value should be 3.14")) {
        return false;
    }

    const auto* label = record.find("label");
    if (!expect_true(label != nullptr && std::holds_alternative<std::string>(label->storage()),
                     "label should be string")) {
        return false;
    }
    if (!expect_true(std::get<std::string>(label->storage()) == "hello", "label should be hello")) {
        return false;
    }

    const auto* big = record.find("big");
    if (!expect_true(big != nullptr && std::holds_alternative<std::int64_t>(big->storage()),
                     "big should be i64")) {
        return false;
    }
    return expect_true(std::get<std::int64_t>(big->storage()) == 9000000000LL,
                       "big should be 9000000000");
}

bool test_array_field() {
    constexpr std::int32_t kTpArrayDouble = 21;

    RecordBuilder builder;
    builder.begin_record({{"data", kTpArrayDouble}});

    // Write Array<double> with shape [2,3] = 6 elements.
    builder.begin_array("Array<double>", {2, 3}, 6);
    for (int index = 1; index <= 6; ++index) {
        builder.write_f64(static_cast<double>(index));
    }

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    const auto* data = record.find("data");
    if (!expect_true(data != nullptr, "data field missing")) {
        return false;
    }

    const auto* arr = std::get_if<casacore_mini::RecordValue::double_array>(&data->storage());
    if (!expect_true(arr != nullptr, "data should be double_array")) {
        return false;
    }
    if (!expect_true(arr->shape.size() == 2U, "shape should have 2 dims")) {
        return false;
    }
    if (!expect_true(arr->shape[0] == 2U && arr->shape[1] == 3U, "shape should be [2,3]")) {
        return false;
    }
    if (!expect_true(arr->elements.size() == 6U, "should have 6 elements")) {
        return false;
    }
    return expect_true(arr->elements[0] == 1.0 && arr->elements[5] == 6.0,
                       "element values mismatch");
}

bool test_complex_fields() {
    constexpr std::int32_t kTpComplex = 9;
    constexpr std::int32_t kTpDComplex = 10;

    RecordBuilder builder;
    builder.begin_record({{"cf", kTpComplex}, {"cd", kTpDComplex}});
    builder.write_complex64(std::complex<float>(1.0F, -2.0F));
    builder.write_complex128(std::complex<double>(3.0, -4.0));

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    const auto* cf = record.find("cf");
    if (!expect_true(cf != nullptr && std::holds_alternative<std::complex<float>>(cf->storage()),
                     "cf should be complex<float>")) {
        return false;
    }
    if (!expect_true(std::get<std::complex<float>>(cf->storage()) ==
                         std::complex<float>(1.0F, -2.0F),
                     "cf value mismatch")) {
        return false;
    }

    const auto* cd = record.find("cd");
    if (!expect_true(cd != nullptr && std::holds_alternative<std::complex<double>>(cd->storage()),
                     "cd should be complex<double>")) {
        return false;
    }
    return expect_true(std::get<std::complex<double>>(cd->storage()) ==
                           std::complex<double>(3.0, -4.0),
                       "cd value mismatch");
}

bool test_nested_sub_record() {
    // Variable sub-record: sub_fields is empty in descriptor, full Record object in payload.
    constexpr std::int32_t kTpRecord = 25;
    constexpr std::int32_t kTpDouble = 8;
    constexpr std::int32_t kTpString = 11;

    casacore_mini::AipsIoWriter writer;

    // Outer Record header.
    writer.write_object_header(0U, "Record", 1U);
    // Outer RecordDesc: one field "child" of type TpRecord.
    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(1); // nfields
    writer.write_string("child");
    writer.write_i32(kTpRecord);
    // Sub-RecordDesc: empty (variable sub-record).
    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(0);
    writer.write_string(""); // comment
    // recordType = Variable.
    writer.write_i32(1);

    // Now write the child as a full Record.
    writer.write_u32(0U);
    writer.write_string("Record");
    writer.write_u32(1U);
    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(2); // 2 fields
    writer.write_string("pi");
    writer.write_i32(kTpDouble);
    writer.write_string(""); // comment
    writer.write_string("name");
    writer.write_i32(kTpString);
    writer.write_string(""); // comment
    writer.write_i32(1);     // recordType
    // Field values.
    writer.write_f64(3.14159);
    writer.write_string("test");

    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    const auto* child_val = record.find("child");
    if (!expect_true(child_val != nullptr, "child field missing")) {
        return false;
    }

    const auto* child_ptr =
        std::get_if<casacore_mini::RecordValue::record_ptr>(&child_val->storage());
    if (!expect_true(child_ptr != nullptr && *child_ptr != nullptr,
                     "child should be a sub-record")) {
        return false;
    }

    const auto& child = **child_ptr;
    if (!expect_true(child.size() == 2U, "child should have 2 fields")) {
        return false;
    }

    const auto* pi = child.find("pi");
    if (!expect_true(pi != nullptr && std::get<double>(pi->storage()) == 3.14159,
                     "pi value mismatch")) {
        return false;
    }

    const auto* name = child.find("name");
    return expect_true(name != nullptr && std::get<std::string>(name->storage()) == "test",
                       "name value mismatch");
}

bool test_rejects_wrong_object_type() {
    casacore_mini::AipsIoWriter writer;
    writer.write_object_header(0U, "Table", 1U);

    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);

    try {
        static_cast<void>(casacore_mini::read_aipsio_record(reader));
    } catch (const std::runtime_error&) {
        return true;
    }
    std::cerr << "reader accepted non-Record object type\n";
    return false;
}

bool test_string_array_field() {
    constexpr std::int32_t kTpArrayString = 24;

    RecordBuilder builder;
    builder.begin_record({{"names", kTpArrayString}});
    builder.begin_array("Array<String>", {3}, 3);
    builder.write_string("alpha");
    builder.write_string("beta");
    builder.write_string("gamma");

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    const auto* names = record.find("names");
    if (!expect_true(names != nullptr, "names field missing")) {
        return false;
    }
    const auto* arr = std::get_if<casacore_mini::RecordValue::string_array>(&names->storage());
    if (!expect_true(arr != nullptr, "names should be string_array")) {
        return false;
    }
    if (!expect_true(arr->elements.size() == 3U, "should have 3 elements")) {
        return false;
    }
    return expect_true(arr->elements[0] == "alpha" && arr->elements[1] == "beta" &&
                           arr->elements[2] == "gamma",
                       "string array values mismatch");
}

bool test_rejects_huge_array_count() {
    // Stream claims millions of elements but only has a few bytes of data.
    constexpr std::int32_t kTpArrayDouble = 21;

    RecordBuilder builder;
    builder.begin_record({{"data", kTpArrayDouble}});
    // Array header with shape [1000000], claiming 1000000 elements, but no actual data.
    builder.begin_array("Array<double>", {1000000}, 1000000);

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);

    try {
        static_cast<void>(casacore_mini::read_aipsio_record(reader));
    } catch (const std::runtime_error&) {
        return true;
    }
    std::cerr << "reader accepted implausible array element count\n";
    return false;
}

bool test_rejects_insufficient_bytes_for_fixed_width_array() {
    constexpr std::int32_t kTpArrayDouble = 21;

    RecordBuilder builder;
    builder.begin_record({{"data", kTpArrayDouble}});
    // Claims 10 elements but only writes 2 doubles of payload.
    builder.begin_array("Array<double>", {10}, 10);
    builder.write_f64(1.0);
    builder.write_f64(2.0);

    auto bytes = builder.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);

    try {
        static_cast<void>(casacore_mini::read_aipsio_record(reader));
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        return expect_true(message.find("plausible stream size") != std::string::npos,
                           "unexpected error for fixed-width array plausibility guard");
    }
    std::cerr << "reader accepted array with impossible fixed-width payload\n";
    return false;
}

bool test_rejects_huge_field_count() {
    // Manually craft a stream with a RecordDesc claiming huge nfields.
    casacore_mini::AipsIoWriter writer;
    writer.write_object_header(0U, "Record", 1U);
    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(999999); // nfields — way more than remaining bytes can support
    // No actual field data follows.

    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);

    try {
        static_cast<void>(casacore_mini::read_aipsio_record(reader));
    } catch (const std::runtime_error&) {
        return true;
    }
    std::cerr << "reader accepted implausible field count\n";
    return false;
}

bool test_rejects_negative_field_type() {
    // Field type code out of known range should throw.
    casacore_mini::AipsIoWriter writer;
    writer.write_object_header(0U, "Record", 1U);
    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(1); // nfields = 1
    writer.write_string("bad_field");
    writer.write_i32(99);    // invalid type code
    writer.write_string(""); // comment
    writer.write_i32(1);     // recordType

    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);

    try {
        static_cast<void>(casacore_mini::read_aipsio_record(reader));
    } catch (const std::runtime_error&) {
        return true;
    }
    std::cerr << "reader accepted invalid field type code\n";
    return false;
}

bool test_accepts_negative_descriptor_iposition() {
    constexpr std::int32_t kTpArrayDouble = 21;

    casacore_mini::AipsIoWriter writer;
    writer.write_object_header(0U, "Record", 1U);

    writer.write_u32(0U);
    writer.write_string("RecordDesc");
    writer.write_u32(2U);
    writer.write_i32(1); // nfields
    writer.write_string("data");
    writer.write_i32(kTpArrayDouble);

    // Descriptor IPosition uses signed wire dimensions and may carry sentinel-like values.
    writer.write_u32(0U);
    writer.write_string("IPosition");
    writer.write_u32(1U);
    writer.write_u32(1U);
    writer.write_i32(-1);

    writer.write_string(""); // comment
    writer.write_i32(1);     // recordType

    writer.write_u32(0U);
    writer.write_string("Array<double>");
    writer.write_u32(3U);
    writer.write_u32(1U); // ndim
    writer.write_u32(1U); // shape[0]
    writer.write_u32(1U); // nelements
    writer.write_f64(42.0);

    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto record = casacore_mini::read_aipsio_record(reader);

    const auto* data = record.find("data");
    if (!expect_true(data != nullptr, "data field missing")) {
        return false;
    }
    const auto* arr = std::get_if<casacore_mini::RecordValue::double_array>(&data->storage());
    if (!expect_true(arr != nullptr, "data should be double_array")) {
        return false;
    }
    return expect_true(arr->shape == std::vector<std::uint64_t>{1U} &&
                           arr->elements == std::vector<double>{42.0},
                       "array payload mismatch after negative descriptor IPosition");
}

} // namespace

int main() noexcept {
    try {
        if (!test_scalar_record()) {
            return 1;
        }
        if (!test_array_field()) {
            return 1;
        }
        if (!test_complex_fields()) {
            return 1;
        }
        if (!test_nested_sub_record()) {
            return 1;
        }
        if (!test_rejects_wrong_object_type()) {
            return 1;
        }
        if (!test_string_array_field()) {
            return 1;
        }
        if (!test_rejects_huge_array_count()) {
            return 1;
        }
        if (!test_rejects_insufficient_bytes_for_fixed_width_array()) {
            return 1;
        }
        if (!test_rejects_huge_field_count()) {
            return 1;
        }
        if (!test_rejects_negative_field_type()) {
            return 1;
        }
        if (!test_accepts_negative_descriptor_iposition()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "aipsio_record_reader_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
