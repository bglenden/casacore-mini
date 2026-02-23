#include "casacore_mini/aipsio_record_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace casacore_mini {
namespace {

/// casacore DataType enum values (from casa/Utilities/DataType.h).
/// The underlying type must be std::int32_t because field type codes are read
/// as 32-bit signed integers from the wire format and values go up to 30.
// NOLINTNEXTLINE(performance-enum-size)
enum class CasacoreType : std::int32_t {
    tp_bool = 0,
    tp_char = 1,
    tp_uchar = 2,
    tp_short = 3,
    tp_ushort = 4,
    tp_int = 5,
    tp_uint = 6,
    tp_float = 7,
    tp_double = 8,
    tp_complex = 9,
    tp_dcomplex = 10,
    tp_string = 11,
    tp_table = 12,
    tp_array_bool = 13,
    tp_array_char = 14,
    tp_array_uchar = 15,
    tp_array_short = 16,
    tp_array_ushort = 17,
    tp_array_int = 18,
    tp_array_uint = 19,
    tp_array_float = 20,
    tp_array_double = 21,
    tp_array_complex = 22,
    tp_array_dcomplex = 23,
    tp_array_string = 24,
    tp_record = 25,
    tp_int64 = 29,
    tp_array_int64 = 30,
};

constexpr std::size_t kMaxRecordDepth = 256;

struct FieldDesc {
    std::string name;
    CasacoreType type{};
    std::vector<std::uint64_t> shape;
    std::vector<FieldDesc> sub_fields;
    std::string comment;
};

/// Read an IPosition from an AipsIO stream.
std::vector<std::uint64_t> read_iposition(AipsIoReader& reader) {
    const auto header = reader.read_object_header();
    if (header.object_type != "IPosition") {
        throw std::runtime_error("expected IPosition, got: " + header.object_type);
    }

    const auto nelements = static_cast<std::size_t>(reader.read_u32());
    std::vector<std::uint64_t> values;
    values.reserve(nelements);
    for (std::size_t index = 0; index < nelements; ++index) {
        if (header.object_version >= 2U) {
            const auto dim = reader.read_i64();
            if (dim < 0) {
                throw std::runtime_error("IPosition dimension is negative");
            }
            values.push_back(static_cast<std::uint64_t>(dim));
        } else {
            const auto dim = reader.read_i32();
            if (dim < 0) {
                throw std::runtime_error("IPosition dimension is negative");
            }
            values.push_back(static_cast<std::uint64_t>(dim));
        }
    }
    return values;
}

/// Read a RecordDesc from an AipsIO stream.
std::vector<FieldDesc> read_record_desc(AipsIoReader& reader) {
    const auto header = reader.read_object_header();
    if (header.object_type != "RecordDesc") {
        throw std::runtime_error("expected RecordDesc, got: " + header.object_type);
    }

    const auto nfields_raw = reader.read_i32();
    if (nfields_raw < 0) {
        throw std::runtime_error("RecordDesc field count is negative");
    }
    const auto nfields = static_cast<std::size_t>(nfields_raw);
    std::vector<FieldDesc> fields;
    fields.reserve(nfields);
    for (std::size_t index = 0; index < nfields; ++index) {
        FieldDesc field;
        field.name = reader.read_string();
        field.type = static_cast<CasacoreType>(reader.read_i32());

        const auto type_i = static_cast<std::int32_t>(field.type);
        const auto is_array_type =
            (type_i >= static_cast<std::int32_t>(CasacoreType::tp_array_bool) &&
             type_i <= static_cast<std::int32_t>(CasacoreType::tp_array_string)) ||
            field.type == CasacoreType::tp_array_int64;

        if (field.type == CasacoreType::tp_record) {
            field.sub_fields = read_record_desc(reader);
        } else if (is_array_type) {
            field.shape = read_iposition(reader);
        } else if (field.type == CasacoreType::tp_table) {
            static_cast<void>(reader.read_string()); // tableDescName, discard
        }

        if (header.object_version >= 2U) {
            field.comment = reader.read_string();
        }

        fields.push_back(std::move(field));
    }
    return fields;
}

/// Read a single Bool value. casacore bit-packs bools, but a single bool is 1 byte.
bool read_bool(AipsIoReader& reader) {
    return reader.read_u8() != 0U;
}

/// Read an Array<T> object and produce a RecordArray.
template <typename element_t, typename reader_fn>
RecordArray<element_t> read_aipsio_array(AipsIoReader& reader, reader_fn&& read_element) {
    const auto header = reader.read_object_header();
    // Accept "Array" or "Array<...>" prefixed names.
    if (header.object_type.rfind("Array", 0) != 0) {
        throw std::runtime_error("expected Array object, got: " + header.object_type);
    }

    const auto ndim = static_cast<std::size_t>(reader.read_u32());

    // Version < 3 has an origin array between ndim and shape — skip it.
    if (header.object_version < 3U) {
        for (std::size_t index = 0; index < ndim; ++index) {
            static_cast<void>(reader.read_i32()); // origin
        }
    }

    RecordArray<element_t> result;
    result.shape.reserve(ndim);
    for (std::size_t index = 0; index < ndim; ++index) {
        result.shape.push_back(static_cast<std::uint64_t>(reader.read_u32()));
    }

    const auto nelements = static_cast<std::size_t>(reader.read_u32());
    result.elements.reserve(nelements);
    for (std::size_t index = 0; index < nelements; ++index) {
        result.elements.push_back(read_element(reader));
    }
    return result;
}

/// Read a Bool Array — bit-packed encoding.
RecordArray<std::int32_t> read_aipsio_bool_array(AipsIoReader& reader) {
    const auto header = reader.read_object_header();
    if (header.object_type.rfind("Array", 0) != 0) {
        throw std::runtime_error("expected Array object, got: " + header.object_type);
    }

    const auto ndim = static_cast<std::size_t>(reader.read_u32());
    if (header.object_version < 3U) {
        for (std::size_t index = 0; index < ndim; ++index) {
            static_cast<void>(reader.read_i32());
        }
    }

    RecordArray<std::int32_t> result;
    result.shape.reserve(ndim);
    for (std::size_t index = 0; index < ndim; ++index) {
        result.shape.push_back(static_cast<std::uint64_t>(reader.read_u32()));
    }

    const auto nelements = static_cast<std::size_t>(reader.read_u32());
    const auto packed_bytes = (nelements + 7U) / 8U;
    result.elements.reserve(nelements);
    std::size_t decoded = 0;
    for (std::size_t byte_index = 0; byte_index < packed_bytes; ++byte_index) {
        const auto byte_val = reader.read_u8();
        for (unsigned int bit = 0; bit < 8U && decoded < nelements; ++bit, ++decoded) {
            result.elements.push_back(((byte_val >> bit) & 1U) != 0U ? 1 : 0);
        }
    }
    return result;
}

Record read_record_fields(AipsIoReader& reader, const std::vector<FieldDesc>& fields,
                          std::size_t depth);

/// Read a single field value based on its type descriptor.
RecordValue read_field_value(AipsIoReader& reader, const FieldDesc& field, std::size_t depth) {
    switch (field.type) {
    case CasacoreType::tp_bool:
        return RecordValue(read_bool(reader));
    case CasacoreType::tp_char:
        return RecordValue(static_cast<std::int16_t>(static_cast<std::int8_t>(reader.read_u8())));
    case CasacoreType::tp_uchar:
        return RecordValue(static_cast<std::uint16_t>(reader.read_u8()));
    case CasacoreType::tp_short:
        return RecordValue(reader.read_i16());
    case CasacoreType::tp_ushort:
        return RecordValue(reader.read_u16());
    case CasacoreType::tp_int:
        return RecordValue(reader.read_i32());
    case CasacoreType::tp_uint:
        return RecordValue(reader.read_u32());
    case CasacoreType::tp_int64:
        return RecordValue(reader.read_i64());
    case CasacoreType::tp_float:
        return RecordValue(reader.read_f32());
    case CasacoreType::tp_double:
        return RecordValue(reader.read_f64());
    case CasacoreType::tp_complex:
        return RecordValue(reader.read_complex64());
    case CasacoreType::tp_dcomplex:
        return RecordValue(reader.read_complex128());
    case CasacoreType::tp_string:
    case CasacoreType::tp_table:
        return RecordValue(reader.read_string());

    case CasacoreType::tp_record: {
        if (field.sub_fields.empty()) {
            // Variable sub-record: full Record object.
            return RecordValue::from_record(read_aipsio_record(reader));
        }
        // Fixed sub-record: bare field values, schema from descriptor.
        return RecordValue::from_record(read_record_fields(reader, field.sub_fields, depth + 1U));
    }

    case CasacoreType::tp_array_bool:
        return RecordValue(read_aipsio_bool_array(reader));
    case CasacoreType::tp_array_short:
        return RecordValue(read_aipsio_array<std::int16_t>(
            reader, [](AipsIoReader& rdr) { return rdr.read_i16(); }));
    case CasacoreType::tp_array_ushort:
        return RecordValue(read_aipsio_array<std::uint16_t>(
            reader, [](AipsIoReader& rdr) { return rdr.read_u16(); }));
    case CasacoreType::tp_array_int:
        return RecordValue(read_aipsio_array<std::int32_t>(
            reader, [](AipsIoReader& rdr) { return rdr.read_i32(); }));
    case CasacoreType::tp_array_uint:
        return RecordValue(read_aipsio_array<std::uint32_t>(
            reader, [](AipsIoReader& rdr) { return rdr.read_u32(); }));
    case CasacoreType::tp_array_int64:
        return RecordValue(read_aipsio_array<std::int64_t>(
            reader, [](AipsIoReader& rdr) { return rdr.read_i64(); }));
    case CasacoreType::tp_array_float:
        return RecordValue(
            read_aipsio_array<float>(reader, [](AipsIoReader& rdr) { return rdr.read_f32(); }));
    case CasacoreType::tp_array_double:
        return RecordValue(
            read_aipsio_array<double>(reader, [](AipsIoReader& rdr) { return rdr.read_f64(); }));
    case CasacoreType::tp_array_complex:
        return RecordValue(read_aipsio_array<std::complex<float>>(
            reader, [](AipsIoReader& rdr) { return rdr.read_complex64(); }));
    case CasacoreType::tp_array_dcomplex:
        return RecordValue(read_aipsio_array<std::complex<double>>(
            reader, [](AipsIoReader& rdr) { return rdr.read_complex128(); }));
    case CasacoreType::tp_array_string:
        return RecordValue(read_aipsio_array<std::string>(
            reader, [](AipsIoReader& rdr) { return rdr.read_string(); }));

    case CasacoreType::tp_array_char:
        // Treat as int16 array (promotion, no exact 8-bit signed type in RecordValue).
        return RecordValue(read_aipsio_array<std::int16_t>(reader, [](AipsIoReader& rdr) {
            return static_cast<std::int16_t>(static_cast<std::int8_t>(rdr.read_u8()));
        }));
    case CasacoreType::tp_array_uchar:
        return RecordValue(read_aipsio_array<std::uint16_t>(
            reader, [](AipsIoReader& rdr) { return static_cast<std::uint16_t>(rdr.read_u8()); }));

    default:
        throw std::runtime_error("unsupported casacore field type: " +
                                 std::to_string(static_cast<std::int32_t>(field.type)));
    }
}

/// Read bare field values (no Record header) given a known field descriptor.
Record read_record_fields(AipsIoReader& reader, const std::vector<FieldDesc>& fields,
                          const std::size_t depth) {
    if (depth > kMaxRecordDepth) {
        throw std::runtime_error("AipsIO Record nesting depth exceeded");
    }
    Record result;
    for (const auto& field : fields) {
        result.set(field.name, read_field_value(reader, field, depth));
    }
    return result;
}

} // namespace

Record read_aipsio_record(AipsIoReader& reader) {
    const auto header = reader.read_object_header();
    if (header.object_type != "Record") {
        throw std::runtime_error("expected Record object, got: " + header.object_type);
    }

    const auto fields = read_record_desc(reader);
    static_cast<void>(reader.read_i32()); // recordType (Fixed=0, Variable=1), not needed

    return read_record_fields(reader, fields, 0U);
}

} // namespace casacore_mini
