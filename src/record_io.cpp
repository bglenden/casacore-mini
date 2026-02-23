#include "casacore_mini/record_io.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ios>
#include <istream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace casacore_mini {
namespace {

enum class value_tag : std::uint8_t {
    boolean = 1,
    int64 = 2,
    float64 = 3,
    string = 4,
    complex128 = 5,
    list = 6,
    record = 7,
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
        value |= static_cast<unsigned_t>(bytes[index]) << shift;
    }
    return value;
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

void write_record_body(const Record& value, std::ostream& output, std::size_t depth);
Record read_record_body(std::istream& input, std::size_t depth);

void write_value(const RecordValue& value, std::ostream& output, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while writing");
    }

    const auto& storage = value.storage();
    if (const auto* as_bool = std::get_if<bool>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::boolean));
        write_unsigned_le(output, static_cast<std::uint8_t>(*as_bool ? 1U : 0U));
        return;
    }

    if (const auto* as_int = std::get_if<std::int64_t>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::int64));
        write_unsigned_le(output,
                          static_cast<std::uint64_t>(std::bit_cast<std::uint64_t>(*as_int)));
        return;
    }

    if (const auto* as_double = std::get_if<double>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::float64));
        write_unsigned_le(output, std::bit_cast<std::uint64_t>(*as_double));
        return;
    }

    if (const auto* as_string = std::get_if<std::string>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::string));
        write_string(output, *as_string);
        return;
    }

    if (const auto* as_complex = std::get_if<std::complex<double>>(&storage)) {
        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::complex128));
        write_unsigned_le(output, std::bit_cast<std::uint64_t>(as_complex->real()));
        write_unsigned_le(output, std::bit_cast<std::uint64_t>(as_complex->imag()));
        return;
    }

    if (const auto* as_list = std::get_if<RecordValue::list_ptr>(&storage)) {
        if (!*as_list) {
            throw std::runtime_error("Record list pointer must not be null while writing");
        }

        write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::list));
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

    write_unsigned_le(output, static_cast<std::uint8_t>(value_tag::record));
    write_record_body(**as_record, output, depth + 1U);
}

RecordValue read_value(std::istream& input, const std::size_t depth) {
    if (depth > kMaxDepth) {
        throw std::runtime_error("binary Record depth limit exceeded while reading");
    }

    const auto tag = static_cast<value_tag>(read_unsigned_le<std::uint8_t>(input));
    switch (tag) {
    case value_tag::boolean:
        return RecordValue(read_unsigned_le<std::uint8_t>(input) != 0U);
    case value_tag::int64:
        return RecordValue(std::bit_cast<std::int64_t>(read_unsigned_le<std::uint64_t>(input)));
    case value_tag::float64:
        return RecordValue(std::bit_cast<double>(read_unsigned_le<std::uint64_t>(input)));
    case value_tag::string:
        return RecordValue(read_string(input));
    case value_tag::complex128: {
        const auto real = std::bit_cast<double>(read_unsigned_le<std::uint64_t>(input));
        const auto imag = std::bit_cast<double>(read_unsigned_le<std::uint64_t>(input));
        return RecordValue(std::complex<double>(real, imag));
    }
    case value_tag::list: {
        const auto count = static_cast<std::size_t>(read_unsigned_le<std::uint32_t>(input));
        RecordList value;
        value.elements.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            value.elements.push_back(read_value(input, depth + 1U));
        }
        return RecordValue::from_list(std::move(value));
    }
    case value_tag::record:
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
