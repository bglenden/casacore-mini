// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/record_io.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace casacore_mini {
namespace {

enum class ValueTag : std::uint8_t {
    boolean = 1,
    int64 = 2,
    float64 = 3,
    string = 4,
    complex128 = 5,
    list = 6,
    record = 7,
    complex64 = 8,
    int16 = 9,
    uint16 = 10,
    int32 = 11,
    uint32 = 12,
    uint64 = 13,
    float32 = 14,
    int16_array = 20,
    uint16_array = 21,
    int32_array = 22,
    uint32_array = 23,
    int64_array = 24,
    uint64_array = 25,
    float32_array = 26,
    float64_array = 27,
    string_array = 28,
    complex64_array = 29,
    complex128_array = 30,
};

constexpr std::array<char, 4> kMagic{{'C', 'C', 'M', 'R'}};
constexpr std::uint8_t kFormatVersion = 1;
constexpr std::size_t kMaxDepth = 1024;

void write_or_throw(std::ostream& output, const void* data, const std::size_t length) {
    output.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
    if (!output) {
        throw std::runtime_error("failed to write binary Record payload");
    }
}

void read_or_throw(std::istream& input, void* data, const std::size_t length) {
    input.read(static_cast<char*>(data), static_cast<std::streamsize>(length));
    if (!input) {
        throw std::runtime_error("failed to read binary Record payload");
    }
}

template <typename unsigned_t>
void write_unsigned_le(std::ostream& output, const unsigned_t value) {
    static_assert(std::is_unsigned_v<unsigned_t>);
    std::array<std::uint8_t, sizeof(unsigned_t)> bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto shift = static_cast<unsigned_t>(index) * static_cast<unsigned_t>(8U);
        bytes[index] = static_cast<std::uint8_t>((value >> shift) & static_cast<unsigned_t>(0xFFU));
    }
    write_or_throw(output, bytes.data(), bytes.size());
}

template <typename unsigned_t> unsigned_t read_unsigned_le(std::istream& input) {
    static_assert(std::is_unsigned_v<unsigned_t>);
    std::array<std::uint8_t, sizeof(unsigned_t)> bytes{};
    read_or_throw(input, bytes.data(), bytes.size());

    unsigned_t value = 0;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const auto shift = static_cast<unsigned_t>(index) * static_cast<unsigned_t>(8U);
        value = static_cast<unsigned_t>(value | (static_cast<unsigned_t>(bytes[index]) << shift));
    }
    return value;
}

template <typename signed_t, typename unsigned_t>
void write_signed_le(std::ostream& output, const signed_t value) {
    static_assert(std::is_signed_v<signed_t>);
    static_assert(std::is_unsigned_v<unsigned_t>);
    static_assert(sizeof(signed_t) == sizeof(unsigned_t));
    write_unsigned_le(output, std::bit_cast<unsigned_t>(value));
}

template <typename signed_t, typename unsigned_t> signed_t read_signed_le(std::istream& input) {
    static_assert(std::is_signed_v<signed_t>);
    static_assert(std::is_unsigned_v<unsigned_t>);
    static_assert(sizeof(signed_t) == sizeof(unsigned_t));
    return std::bit_cast<signed_t>(read_unsigned_le<unsigned_t>(input));
}

void write_float32(std::ostream& output, const float value) {
    write_unsigned_le(output, std::bit_cast<std::uint32_t>(value));
}

float read_float32(std::istream& input) {
    return std::bit_cast<float>(read_unsigned_le<std::uint32_t>(input));
}

void write_float64(std::ostream& output, const double value) {
    write_unsigned_le(output, std::bit_cast<std::uint64_t>(value));
}

double read_float64(std::istream& input) {
    return std::bit_cast<double>(read_unsigned_le<std::uint64_t>(input));
}

void write_string(std::ostream& output, const std::string& value) {
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("string too large for binary Record encoding");
    }

    write_unsigned_le(output, static_cast<std::uint32_t>(value.size()));
    write_or_throw(output, value.data(), value.size());
}

std::string read_string(std::istream& input) {
    const auto size = static_cast<std::size_t>(read_unsigned_le<std::uint32_t>(input));
    std::string value(size, '\0');
    if (size > 0) {
        read_or_throw(input, value.data(), size);
    }
    return value;
}

std::size_t checked_size_from_u64(const std::uint64_t value, const char* context) {
    if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error(std::string(context) + " exceeds host size limits");
    }
    return static_cast<std::size_t>(value);
}

std::uint64_t shape_element_count_u64(const std::vector<std::uint64_t>& shape) {
    if (shape.empty()) {
        return 0U;
    }

    std::uint64_t product = 1U;
    for (const auto dim : shape) {
        if (dim == 0U) {
            return 0U;
        }
        if (product > std::numeric_limits<std::uint64_t>::max() / dim) {
            throw std::runtime_error("Record array shape product overflow");
        }
        product *= dim;
    }
    return product;
}

void write_array_shape(std::ostream& output, const std::vector<std::uint64_t>& shape) {
    if (shape.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("Record array rank too large for binary Record encoding");
    }
    write_unsigned_le(output, static_cast<std::uint32_t>(shape.size()));
    for (const auto dim : shape) {
        write_unsigned_le(output, dim);
    }
}

std::vector<std::uint64_t> read_array_shape(std::istream& input) {
    const auto rank = static_cast<std::size_t>(read_unsigned_le<std::uint32_t>(input));
    std::vector<std::uint64_t> shape;
    shape.reserve(rank);
    for (std::size_t index = 0; index < rank; ++index) {
        shape.push_back(read_unsigned_le<std::uint64_t>(input));
    }
    return shape;
}

template <typename element_t, typename writer_t>
void write_array(std::ostream& output, const RecordArray<element_t>& value,
                 writer_t&& write_element) {
    write_array_shape(output, value.shape);
    write_unsigned_le(output, static_cast<std::uint64_t>(value.elements.size()));
    for (const auto& element : value.elements) {
        write_element(output, element);
    }
}

template <typename element_t, typename reader_t>
RecordArray<element_t> read_array(std::istream& input, reader_t&& read_element) {
    RecordArray<element_t> value;
    value.shape = read_array_shape(input);

    const auto expected_count = shape_element_count_u64(value.shape);
    const auto stored_count = read_unsigned_le<std::uint64_t>(input);
    if (stored_count != expected_count) {
        throw std::runtime_error("Record array element count does not match shape");
    }

    const auto count = checked_size_from_u64(stored_count, "Record array element count");
    value.elements.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        value.elements.push_back(read_element(input));
    }
    return value;
}

void write_record_body(const Record& value, std::ostream& output, std::size_t depth);
Record read_record_body(std::istream& input, std::size_t depth);

void write_value(const RecordValue& record_value, std::ostream& output, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while writing");
    }

    const auto& storage = record_value.storage();
    if (const auto* as_bool = std::get_if<bool>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::boolean));
        write_unsigned_le(output, static_cast<std::uint8_t>(*as_bool ? 1U : 0U));
        return;
    }

    if (const auto* as_int = std::get_if<std::int16_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int16));
        write_signed_le<std::int16_t, std::uint16_t>(output, *as_int);
        return;
    }

    if (const auto* as_int = std::get_if<std::uint16_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint16));
        write_unsigned_le(output, *as_int);
        return;
    }

    if (const auto* as_int = std::get_if<std::int32_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int32));
        write_signed_le<std::int32_t, std::uint32_t>(output, *as_int);
        return;
    }

    if (const auto* as_int = std::get_if<std::uint32_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint32));
        write_unsigned_le(output, *as_int);
        return;
    }

    if (const auto* as_int = std::get_if<std::int64_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int64));
        write_signed_le<std::int64_t, std::uint64_t>(output, *as_int);
        return;
    }

    if (const auto* as_int = std::get_if<std::uint64_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint64));
        write_unsigned_le(output, *as_int);
        return;
    }

    if (const auto* as_float = std::get_if<float>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::float32));
        write_float32(output, *as_float);
        return;
    }

    if (const auto* as_double = std::get_if<double>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::float64));
        write_float64(output, *as_double);
        return;
    }

    if (const auto* as_string = std::get_if<std::string>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::string));
        write_string(output, *as_string);
        return;
    }

    if (const auto* as_complex = std::get_if<std::complex<float>>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::complex64));
        write_float32(output, as_complex->real());
        write_float32(output, as_complex->imag());
        return;
    }

    if (const auto* as_complex = std::get_if<std::complex<double>>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::complex128));
        write_float64(output, as_complex->real());
        write_float64(output, as_complex->imag());
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::int16_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int16_array));
        write_array(output, *as_array, [](std::ostream& out, const std::int16_t value) {
            write_signed_le<std::int16_t, std::uint16_t>(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::uint16_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint16_array));
        write_array(output, *as_array, [](std::ostream& out, const std::uint16_t value) {
            write_unsigned_le(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::int32_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int32_array));
        write_array(output, *as_array, [](std::ostream& out, const std::int32_t value) {
            write_signed_le<std::int32_t, std::uint32_t>(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::uint32_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint32_array));
        write_array(output, *as_array, [](std::ostream& out, const std::uint32_t value) {
            write_unsigned_le(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::int64_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::int64_array));
        write_array(output, *as_array, [](std::ostream& out, const std::int64_t value) {
            write_signed_le<std::int64_t, std::uint64_t>(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::uint64_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::uint64_array));
        write_array(output, *as_array, [](std::ostream& out, const std::uint64_t value) {
            write_unsigned_le(out, value);
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::float_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::float32_array));
        write_array(output, *as_array,
                    [](std::ostream& out, const float value) { write_float32(out, value); });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::double_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::float64_array));
        write_array(output, *as_array,
                    [](std::ostream& out, const double value) { write_float64(out, value); });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::string_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::string_array));
        write_array(output, *as_array,
                    [](std::ostream& out, const std::string& value) { write_string(out, value); });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::complex64_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::complex64_array));
        write_array(output, *as_array, [](std::ostream& out, const std::complex<float>& value) {
            write_float32(out, value.real());
            write_float32(out, value.imag());
        });
        return;
    }

    if (const auto* as_array = std::get_if<RecordValue::complex128_array>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::complex128_array));
        write_array(output, *as_array, [](std::ostream& out, const std::complex<double>& value) {
            write_float64(out, value.real());
            write_float64(out, value.imag());
        });
        return;
    }

    if (const auto* as_list = std::get_if<RecordValue::list_ptr>(&storage)) {
        if (!*as_list) {
            throw std::runtime_error("Record list pointer must not be null while writing");
        }

        write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::list));
        const auto& elements = (*as_list)->elements;
        if (elements.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw std::runtime_error("Record list too large for binary Record encoding");
        }

        write_unsigned_le(output, static_cast<std::uint32_t>(elements.size()));
        for (const auto& element : elements) {
            write_value(element, output, depth + 1U);
        }
        return;
    }

    const auto* as_record = std::get_if<RecordValue::record_ptr>(&storage);
    if (as_record == nullptr || *as_record == nullptr) {
        throw std::runtime_error("Record pointer must not be null while writing");
    }

    write_unsigned_le(output, static_cast<std::uint8_t>(ValueTag::record));
    write_record_body(**as_record, output, depth + 1U);
}

RecordValue read_value(std::istream& input, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while reading");
    }

    const auto tag = static_cast<ValueTag>(read_unsigned_le<std::uint8_t>(input));
    switch (tag) {
    case ValueTag::boolean:
        return RecordValue(read_unsigned_le<std::uint8_t>(input) != 0U);
    case ValueTag::int16:
        return RecordValue(read_signed_le<std::int16_t, std::uint16_t>(input));
    case ValueTag::uint16:
        return RecordValue(read_unsigned_le<std::uint16_t>(input));
    case ValueTag::int32:
        return RecordValue(read_signed_le<std::int32_t, std::uint32_t>(input));
    case ValueTag::uint32:
        return RecordValue(read_unsigned_le<std::uint32_t>(input));
    case ValueTag::int64:
        return RecordValue(read_signed_le<std::int64_t, std::uint64_t>(input));
    case ValueTag::uint64:
        return RecordValue(read_unsigned_le<std::uint64_t>(input));
    case ValueTag::float32:
        return RecordValue(read_float32(input));
    case ValueTag::float64:
        return RecordValue(read_float64(input));
    case ValueTag::string:
        return RecordValue(read_string(input));
    case ValueTag::complex64: {
        const auto real = read_float32(input);
        const auto imag = read_float32(input);
        return RecordValue(std::complex<float>(real, imag));
    }
    case ValueTag::complex128: {
        const auto real = read_float64(input);
        const auto imag = read_float64(input);
        return RecordValue(std::complex<double>(real, imag));
    }
    case ValueTag::int16_array:
        return RecordValue(read_array<std::int16_t>(input, [](std::istream& in) {
            return read_signed_le<std::int16_t, std::uint16_t>(in);
        }));
    case ValueTag::uint16_array:
        return RecordValue(read_array<std::uint16_t>(
            input, [](std::istream& in) { return read_unsigned_le<std::uint16_t>(in); }));
    case ValueTag::int32_array:
        return RecordValue(read_array<std::int32_t>(input, [](std::istream& in) {
            return read_signed_le<std::int32_t, std::uint32_t>(in);
        }));
    case ValueTag::uint32_array:
        return RecordValue(read_array<std::uint32_t>(
            input, [](std::istream& in) { return read_unsigned_le<std::uint32_t>(in); }));
    case ValueTag::int64_array:
        return RecordValue(read_array<std::int64_t>(input, [](std::istream& in) {
            return read_signed_le<std::int64_t, std::uint64_t>(in);
        }));
    case ValueTag::uint64_array:
        return RecordValue(read_array<std::uint64_t>(
            input, [](std::istream& in) { return read_unsigned_le<std::uint64_t>(in); }));
    case ValueTag::float32_array:
        return RecordValue(
            read_array<float>(input, [](std::istream& in) { return read_float32(in); }));
    case ValueTag::float64_array:
        return RecordValue(
            read_array<double>(input, [](std::istream& in) { return read_float64(in); }));
    case ValueTag::string_array:
        return RecordValue(
            read_array<std::string>(input, [](std::istream& in) { return read_string(in); }));
    case ValueTag::complex64_array:
        return RecordValue(read_array<std::complex<float>>(input, [](std::istream& in) {
            const auto real = read_float32(in);
            const auto imag = read_float32(in);
            return std::complex<float>(real, imag);
        }));
    case ValueTag::complex128_array:
        return RecordValue(read_array<std::complex<double>>(input, [](std::istream& in) {
            const auto real = read_float64(in);
            const auto imag = read_float64(in);
            return std::complex<double>(real, imag);
        }));
    case ValueTag::list: {
        const auto count = static_cast<std::size_t>(read_unsigned_le<std::uint32_t>(input));
        RecordList value;
        value.elements.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            value.elements.push_back(read_value(input, depth + 1U));
        }
        return RecordValue::from_list(std::move(value));
    }
    case ValueTag::record:
        return RecordValue::from_record(read_record_body(input, depth + 1U));
    default:
        throw std::runtime_error("unsupported value tag in binary Record payload");
    }
}

void write_record_body(const Record& value, std::ostream& output, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while writing");
    }

    const auto& entries = value.entries();
    if (entries.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("Record has too many fields for binary Record encoding");
    }

    write_unsigned_le(output, static_cast<std::uint32_t>(entries.size()));
    for (const auto& [key, item] : entries) {
        write_string(output, key);
        write_value(item, output, depth + 1U);
    }
}

Record read_record_body(std::istream& input, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while reading");
    }

    const auto count = static_cast<std::size_t>(read_unsigned_le<std::uint32_t>(input));
    Record value;
    for (std::size_t index = 0; index < count; ++index) {
        const auto key = read_string(input);
        value.set(key, read_value(input, depth + 1U));
    }
    return value;
}

} // namespace

void write_record_binary(const Record& value, std::ostream& output) {
    write_or_throw(output, kMagic.data(), kMagic.size());
    write_unsigned_le(output, kFormatVersion);
    write_record_body(value, output, 0U);
}

Record read_record_binary(std::istream& input) {
    std::array<char, kMagic.size()> magic{};
    read_or_throw(input, magic.data(), magic.size());
    if (magic != kMagic) {
        throw std::runtime_error("invalid binary Record magic header");
    }

    const auto version = read_unsigned_le<std::uint8_t>(input);
    if (version != kFormatVersion) {
        throw std::runtime_error("unsupported binary Record format version");
    }

    return read_record_body(input, 0U);
}

std::vector<std::uint8_t> serialize_record_binary(const Record& value) {
    std::ostringstream output(std::ios::binary);
    write_record_binary(value, output);
    const auto bytes = output.str();
    return std::vector<std::uint8_t>(bytes.begin(), bytes.end());
}

Record deserialize_record_binary(const std::span<const std::uint8_t> bytes) {
    std::string payload;
    payload.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    std::istringstream input(payload, std::ios::binary);
    const auto value = read_record_binary(input);

    if (input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("binary Record payload contains trailing bytes");
    }

    return value;
}

} // namespace casacore_mini
