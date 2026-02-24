#include "casacore_mini/aipsio_record_writer.hpp"

#include "casacore_mini/aipsio_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace casacore_mini {
namespace {

constexpr std::size_t kMaxWriteDepth = 256;

// Public Record array shapes use uint64_t extents; wire IPosition remains
// signed to preserve casacore-compatible descriptor semantics.
using public_shape = std::vector<std::uint64_t>;
using wire_i_position = std::vector<std::int64_t>;

[[nodiscard]] std::uint32_t checked_u32(const std::size_t value, std::string_view field_name) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::uint32_t checked_u32_from_u64(const std::uint64_t value,
                                                 std::string_view field_name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] std::int32_t checked_i32(const std::size_t value, std::string_view field_name) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds int32 range");
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t checked_i32(const std::int64_t value, std::string_view field_name) {
    if (value < 0 || value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds int32 range");
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int64_t checked_i64(const std::uint64_t value, std::string_view field_name) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        throw std::runtime_error(std::string(field_name) + " exceeds int64 range");
    }
    return static_cast<std::int64_t>(value);
}

/// casacore DataType codes matching the reader's CasacoreType enum.
namespace casacore_type {
constexpr std::int32_t kBool = 0;
constexpr std::int32_t kShort = 3;
constexpr std::int32_t kUShort = 4;
constexpr std::int32_t kInt = 5;
constexpr std::int32_t kUInt = 6;
constexpr std::int32_t kFloat = 7;
constexpr std::int32_t kDouble = 8;
constexpr std::int32_t kComplex = 9;
constexpr std::int32_t kDComplex = 10;
constexpr std::int32_t kString = 11;
constexpr std::int32_t kArrayShort = 16;
constexpr std::int32_t kArrayUShort = 17;
constexpr std::int32_t kArrayInt = 18;
constexpr std::int32_t kArrayUInt = 19;
constexpr std::int32_t kArrayFloat = 20;
constexpr std::int32_t kArrayDouble = 21;
constexpr std::int32_t kArrayComplex = 22;
constexpr std::int32_t kArrayDComplex = 23;
constexpr std::int32_t kArrayString = 24;
constexpr std::int32_t kRecord = 25;
constexpr std::int32_t kInt64 = 29;
constexpr std::int32_t kArrayInt64 = 30;
} // namespace casacore_type

/// Map a RecordValue variant to the casacore DataType code.
[[nodiscard]] std::int32_t casacore_type_for(const RecordValue& value) {
    const auto& storage = value.storage();

    if (std::holds_alternative<bool>(storage)) {
        return casacore_type::kBool;
    }
    if (std::holds_alternative<std::int16_t>(storage)) {
        return casacore_type::kShort;
    }
    if (std::holds_alternative<std::uint16_t>(storage)) {
        return casacore_type::kUShort;
    }
    if (std::holds_alternative<std::int32_t>(storage)) {
        return casacore_type::kInt;
    }
    if (std::holds_alternative<std::uint32_t>(storage)) {
        return casacore_type::kUInt;
    }
    if (std::holds_alternative<std::int64_t>(storage)) {
        return casacore_type::kInt64;
    }
    if (std::holds_alternative<std::uint64_t>(storage)) {
        throw std::runtime_error("uint64_t has no casacore DataType encoding");
    }
    if (std::holds_alternative<float>(storage)) {
        return casacore_type::kFloat;
    }
    if (std::holds_alternative<double>(storage)) {
        return casacore_type::kDouble;
    }
    if (std::holds_alternative<std::complex<float>>(storage)) {
        return casacore_type::kComplex;
    }
    if (std::holds_alternative<std::complex<double>>(storage)) {
        return casacore_type::kDComplex;
    }
    if (std::holds_alternative<std::string>(storage)) {
        return casacore_type::kString;
    }
    if (std::holds_alternative<RecordValue::int16_array>(storage)) {
        return casacore_type::kArrayShort;
    }
    if (std::holds_alternative<RecordValue::uint16_array>(storage)) {
        return casacore_type::kArrayUShort;
    }
    if (std::holds_alternative<RecordValue::int32_array>(storage)) {
        return casacore_type::kArrayInt;
    }
    if (std::holds_alternative<RecordValue::uint32_array>(storage)) {
        return casacore_type::kArrayUInt;
    }
    if (std::holds_alternative<RecordValue::int64_array>(storage)) {
        return casacore_type::kArrayInt64;
    }
    if (std::holds_alternative<RecordValue::float_array>(storage)) {
        return casacore_type::kArrayFloat;
    }
    if (std::holds_alternative<RecordValue::double_array>(storage)) {
        return casacore_type::kArrayDouble;
    }
    if (std::holds_alternative<RecordValue::complex64_array>(storage)) {
        return casacore_type::kArrayComplex;
    }
    if (std::holds_alternative<RecordValue::complex128_array>(storage)) {
        return casacore_type::kArrayDComplex;
    }
    if (std::holds_alternative<RecordValue::string_array>(storage)) {
        return casacore_type::kArrayString;
    }
    if (std::holds_alternative<RecordValue::record_ptr>(storage)) {
        return casacore_type::kRecord;
    }
    throw std::runtime_error("RecordValue type has no casacore DataType encoding");
}

/// Write object header with a placeholder length, returning offset of the length field.
[[nodiscard]] std::size_t begin_object(AipsIoWriter& writer, std::string_view type_name,
                                       std::uint32_t version, const bool include_magic) {
    if (include_magic) {
        writer.write_u32(kAipsIoMagic);
    }
    const auto length_offset = writer.size();
    writer.write_u32(0); // placeholder
    writer.write_string(type_name);
    writer.write_u32(version);
    return length_offset;
}

/// Patch the object length field to casacore AipsIO semantics.
///
/// The stored length includes the 4-byte length field itself (i.e. object
/// bytes excluding only the optional leading magic word).
void end_object(AipsIoWriter& writer, std::size_t length_offset) {
    const auto length = writer.size() - length_offset;
    writer.patch_u32(length_offset, checked_u32(length, "AipsIO object length"));
}

[[nodiscard]] wire_i_position to_wire_iposition(const public_shape& shape) {
    wire_i_position wire_shape;
    wire_shape.reserve(shape.size());
    for (const auto dim : shape) {
        wire_shape.push_back(checked_i64(dim, "IPosition dimension"));
    }
    return wire_shape;
}

/// Write an IPosition (version 1, i32 dimensions).
void write_iposition(AipsIoWriter& writer, const wire_i_position& shape) {
    const auto length_offset = begin_object(writer, "IPosition", 1U, false);
    writer.write_u32(checked_u32(shape.size(), "IPosition rank"));
    for (const auto dim : shape) {
        writer.write_i32(checked_i32(dim, "IPosition dimension"));
    }
    end_object(writer, length_offset);
}

/// Extract shape from a RecordValue that holds an array alternative.
[[nodiscard]] const public_shape& get_array_shape(const RecordValue& value) {
    const auto& storage = value.storage();
    if (const auto* a = std::get_if<RecordValue::int16_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::uint16_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::int32_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::uint32_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::int64_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::float_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::double_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::complex64_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::complex128_array>(&storage)) {
        return a->shape;
    }
    if (const auto* a = std::get_if<RecordValue::string_array>(&storage)) {
        return a->shape;
    }
    // uint64_array is not writable to casacore, but extract shape anyway.
    if (const auto* a = std::get_if<RecordValue::uint64_array>(&storage)) {
        return a->shape;
    }
    throw std::runtime_error("get_array_shape called on non-array RecordValue");
}

// Forward declarations for mutual recursion.
void write_record_impl(AipsIoWriter& writer, const Record& record, std::size_t depth,
                       bool include_magic);
void write_field_value_tr(AipsIoWriter& writer, const RecordValue& value, std::size_t depth,
                          bool as_table_record);
void write_record_tr(AipsIoWriter& writer, const Record& record, std::size_t depth,
                     bool as_table_record);

/// Write RecordDesc (version 2 with comments).
void write_record_desc(AipsIoWriter& writer, const Record& record, const std::size_t depth) {
    if (depth > kMaxWriteDepth) {
        throw std::runtime_error("AipsIO Record write nesting depth exceeded");
    }

    const auto length_offset = begin_object(writer, "RecordDesc", 2U, false);
    writer.write_i32(checked_i32(record.size(), "RecordDesc field count"));

    for (const auto& [name, value] : record.entries()) {
        writer.write_string(name);
        const auto type_code = casacore_type_for(value);
        writer.write_i32(type_code);

        const bool is_array_type =
            (type_code >= casacore_type::kArrayShort && type_code <= casacore_type::kArrayString) ||
            type_code == casacore_type::kArrayInt64;
        if (type_code == casacore_type::kRecord) {
            // Variable sub-record: empty RecordDesc.
            const auto sub_length_offset = begin_object(writer, "RecordDesc", 2U, false);
            writer.write_i32(0);
            end_object(writer, sub_length_offset);
        } else if (is_array_type) {
            write_iposition(writer, to_wire_iposition(get_array_shape(value)));
        }

        writer.write_string(""); // comment (version 2)
    }

    end_object(writer, length_offset);
}

/// Write an Array<T> value (version 3 format: ndim, shape, nelements, elements).
template <typename element_t, typename write_fn>
void write_aipsio_array(AipsIoWriter& writer, std::string_view type_name,
                        const RecordArray<element_t>& array, write_fn&& write_element) {
    const auto length_offset = begin_object(writer, type_name, 3U, false);

    writer.write_u32(checked_u32(array.shape.size(), "Array rank"));
    for (const auto dim : array.shape) {
        writer.write_u32(checked_u32_from_u64(dim, "Array dimension"));
    }
    writer.write_u32(checked_u32(array.elements.size(), "Array element count"));

    for (const auto& element : array.elements) {
        write_element(writer, element);
    }

    end_object(writer, length_offset);
}

/// Write a single field value.
void write_field_value(AipsIoWriter& writer, const RecordValue& value, const std::size_t depth) {
    const auto& storage = value.storage();

    if (const auto* v = std::get_if<bool>(&storage)) {
        writer.write_u8(*v ? 1U : 0U);
    } else if (const auto* v16 = std::get_if<std::int16_t>(&storage)) {
        writer.write_i16(*v16);
    } else if (const auto* vu16 = std::get_if<std::uint16_t>(&storage)) {
        writer.write_u16(*vu16);
    } else if (const auto* vi32 = std::get_if<std::int32_t>(&storage)) {
        writer.write_i32(*vi32);
    } else if (const auto* vu32 = std::get_if<std::uint32_t>(&storage)) {
        writer.write_u32(*vu32);
    } else if (const auto* vi64 = std::get_if<std::int64_t>(&storage)) {
        writer.write_i64(*vi64);
    } else if (const auto* vf = std::get_if<float>(&storage)) {
        writer.write_f32(*vf);
    } else if (const auto* vd = std::get_if<double>(&storage)) {
        writer.write_f64(*vd);
    } else if (const auto* vc = std::get_if<std::complex<float>>(&storage)) {
        writer.write_complex64(*vc);
    } else if (const auto* vdc = std::get_if<std::complex<double>>(&storage)) {
        writer.write_complex128(*vdc);
    } else if (const auto* vs = std::get_if<std::string>(&storage)) {
        writer.write_string(*vs);
    } else if (const auto* arr_i16 = std::get_if<RecordValue::int16_array>(&storage)) {
        write_aipsio_array<std::int16_t>(writer, "Array<Short>", *arr_i16,
                                         [](AipsIoWriter& w, std::int16_t e) { w.write_i16(e); });
    } else if (const auto* arr_u16 = std::get_if<RecordValue::uint16_array>(&storage)) {
        write_aipsio_array<std::uint16_t>(writer, "Array<uShort>", *arr_u16,
                                          [](AipsIoWriter& w, std::uint16_t e) { w.write_u16(e); });
    } else if (const auto* arr_i32 = std::get_if<RecordValue::int32_array>(&storage)) {
        write_aipsio_array<std::int32_t>(writer, "Array<Int>", *arr_i32,
                                         [](AipsIoWriter& w, std::int32_t e) { w.write_i32(e); });
    } else if (const auto* arr_u32 = std::get_if<RecordValue::uint32_array>(&storage)) {
        write_aipsio_array<std::uint32_t>(writer, "Array<uInt>", *arr_u32,
                                          [](AipsIoWriter& w, std::uint32_t e) { w.write_u32(e); });
    } else if (const auto* arr_i64 = std::get_if<RecordValue::int64_array>(&storage)) {
        write_aipsio_array<std::int64_t>(writer, "Array<Int64>", *arr_i64,
                                         [](AipsIoWriter& w, std::int64_t e) { w.write_i64(e); });
    } else if (const auto* arr_f = std::get_if<RecordValue::float_array>(&storage)) {
        write_aipsio_array<float>(writer, "Array<float>", *arr_f,
                                  [](AipsIoWriter& w, float e) { w.write_f32(e); });
    } else if (const auto* arr_d = std::get_if<RecordValue::double_array>(&storage)) {
        write_aipsio_array<double>(writer, "Array<double>", *arr_d,
                                   [](AipsIoWriter& w, double e) { w.write_f64(e); });
    } else if (const auto* arr_c = std::get_if<RecordValue::complex64_array>(&storage)) {
        write_aipsio_array<std::complex<float>>(
            writer, "Array<Complex>", *arr_c,
            [](AipsIoWriter& w, std::complex<float> e) { w.write_complex64(e); });
    } else if (const auto* arr_dc = std::get_if<RecordValue::complex128_array>(&storage)) {
        write_aipsio_array<std::complex<double>>(
            writer, "Array<DComplex>", *arr_dc,
            [](AipsIoWriter& w, std::complex<double> e) { w.write_complex128(e); });
    } else if (const auto* arr_s = std::get_if<RecordValue::string_array>(&storage)) {
        write_aipsio_array<std::string>(
            writer, "Array<String>", *arr_s,
            [](AipsIoWriter& w, const std::string& e) { w.write_string(e); });
    } else if (const auto* rec = std::get_if<RecordValue::record_ptr>(&storage)) {
        if (!*rec) {
            throw std::runtime_error("null record_ptr in RecordValue");
        }
        write_record_impl(writer, **rec, depth + 1U, false);
    } else {
        throw std::runtime_error("unsupported RecordValue type for AipsIO encoding");
    }
}

/// Internal implementation with depth tracking.
void write_record_impl(AipsIoWriter& writer, const Record& record, const std::size_t depth,
                       const bool include_magic) {
    const auto length_offset = begin_object(writer, "Record", 1U, include_magic);
    write_record_desc(writer, record, depth);
    writer.write_i32(1); // recordType = Variable
    for (const auto& [name, value] : record.entries()) {
        write_field_value(writer, value, depth);
    }
    end_object(writer, length_offset);
}

/// Write a single field value, optionally wrapping sub-records as TableRecord.
void write_field_value_tr(AipsIoWriter& writer, const RecordValue& value, const std::size_t depth,
                          const bool as_table_record) {
    const auto& storage = value.storage();
    if (const auto* rec = std::get_if<RecordValue::record_ptr>(&storage)) {
        if (!*rec) {
            throw std::runtime_error("null record_ptr in RecordValue");
        }
        write_record_tr(writer, **rec, depth + 1U, as_table_record);
    } else {
        // Non-record fields are identical.
        write_field_value(writer, value, depth);
    }
}

/// Write a record object header as either "Record" or "TableRecord".
void write_record_tr(AipsIoWriter& writer, const Record& record, const std::size_t depth,
                     const bool as_table_record) {
    const auto type_name = as_table_record ? "TableRecord" : "Record";
    const auto length_offset = begin_object(writer, type_name, 1U, false);
    write_record_desc(writer, record, depth);
    writer.write_i32(1); // recordType = Variable
    for (const auto& [name, value] : record.entries()) {
        write_field_value_tr(writer, value, depth, as_table_record);
    }
    end_object(writer, length_offset);
}

} // namespace

void write_aipsio_record(AipsIoWriter& writer, const Record& record) {
    write_record_impl(writer, record, 0U, true);
}

void write_aipsio_embedded_record(AipsIoWriter& writer, const Record& record) {
    write_record_impl(writer, record, 0U, false);
}

void write_aipsio_record_body(AipsIoWriter& writer, const Record& record) {
    // Write RecordDesc + recordType + field values directly (no Record header).
    write_record_desc(writer, record, 0U);
    writer.write_i32(1); // recordType = Variable
    for (const auto& [name, value] : record.entries()) {
        write_field_value(writer, value, 0U);
    }
}

void write_aipsio_table_record_body(AipsIoWriter& writer, const Record& record) {
    // Like write_aipsio_record_body but sub-records are written as "TableRecord".
    write_record_desc(writer, record, 0U);
    writer.write_i32(1); // recordType = Variable
    for (const auto& [name, value] : record.entries()) {
        write_field_value_tr(writer, value, 0U, true);
    }
}

} // namespace casacore_mini
