#include "casacore_mini/aipsio_reader.hpp"

#include <bit>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace casacore_mini {
namespace {

template <typename unsigned_t>
[[nodiscard]] unsigned_t decode_unsigned_be(const std::span<const std::uint8_t> bytes) {
    static_assert(std::is_unsigned_v<unsigned_t>);
    if (bytes.size() != sizeof(unsigned_t)) {
        throw std::runtime_error("AipsIO decode width mismatch");
    }

    unsigned_t value = 0;
    for (const std::uint8_t byte : bytes) {
        value = static_cast<unsigned_t>((value << 8U) | static_cast<unsigned_t>(byte));
    }
    return value;
}

} // namespace

AipsIoReader::AipsIoReader(const std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

std::size_t AipsIoReader::position() const noexcept {
    return offset_;
}

std::size_t AipsIoReader::remaining() const noexcept {
    return bytes_.size() - offset_;
}

bool AipsIoReader::empty() const noexcept {
    return remaining() == 0U;
}

void AipsIoReader::skip(const std::size_t byte_count) {
    static_cast<void>(read_bytes(byte_count));
}

std::span<const std::uint8_t> AipsIoReader::read_bytes(const std::size_t byte_count) {
    if (byte_count > remaining()) {
        throw std::runtime_error("AipsIO read exceeds available bytes");
    }

    const auto chunk = bytes_.subspan(offset_, byte_count);
    offset_ += byte_count;
    return chunk;
}

std::uint8_t AipsIoReader::read_u8() {
    const auto bytes = read_bytes(sizeof(std::uint8_t));
    return bytes[0];
}

std::int16_t AipsIoReader::read_i16() {
    return std::bit_cast<std::int16_t>(read_u16());
}

std::uint16_t AipsIoReader::read_u16() {
    return decode_unsigned_be<std::uint16_t>(read_bytes(sizeof(std::uint16_t)));
}

std::int32_t AipsIoReader::read_i32() {
    return std::bit_cast<std::int32_t>(read_u32());
}

std::uint32_t AipsIoReader::read_u32() {
    return decode_unsigned_be<std::uint32_t>(read_bytes(sizeof(std::uint32_t)));
}

std::int64_t AipsIoReader::read_i64() {
    return std::bit_cast<std::int64_t>(read_u64());
}

std::uint64_t AipsIoReader::read_u64() {
    return decode_unsigned_be<std::uint64_t>(read_bytes(sizeof(std::uint64_t)));
}

float AipsIoReader::read_f32() {
    return std::bit_cast<float>(read_u32());
}

double AipsIoReader::read_f64() {
    return std::bit_cast<double>(read_u64());
}

std::complex<float> AipsIoReader::read_complex64() {
    const float real = read_f32();
    const float imag = read_f32();
    return std::complex<float>(real, imag);
}

std::complex<double> AipsIoReader::read_complex128() {
    const double real = read_f64();
    const double imag = read_f64();
    return std::complex<double>(real, imag);
}

std::string AipsIoReader::read_string() {
    const auto size_u32 = read_u32();
    if (size_u32 > static_cast<std::uint32_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("AipsIO string size does not fit size_t");
    }
    const auto size = static_cast<std::size_t>(size_u32);
    if (size > remaining()) {
        throw std::runtime_error("AipsIO string length exceeds remaining bytes");
    }
    const auto bytes = read_bytes(size);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

AipsIoObjectHeader AipsIoReader::read_object_header() {
    const auto magic = read_u32();
    if (magic != kAipsIoMagic) {
        throw std::runtime_error("invalid AipsIO magic value");
    }

    AipsIoObjectHeader header;
    header.object_length = read_u32();
    header.object_type = read_string();
    header.object_version = read_u32();
    return header;
}

AipsIoObjectHeader AipsIoReader::read_nested_object_header() {
    AipsIoObjectHeader header;
    header.object_length = read_u32();
    header.object_type = read_string();
    header.object_version = read_u32();
    return header;
}

AipsIoObjectHeader AipsIoReader::read_object_header_auto() {
    // Peek at next 4 bytes to check for magic.
    if (remaining() >= 4) {
        const auto maybe_magic = read_u32();
        if (maybe_magic == kAipsIoMagic) {
            // Root-level header: magic already consumed, read the rest.
            AipsIoObjectHeader header;
            header.object_length = read_u32();
            header.object_type = read_string();
            header.object_version = read_u32();
            return header;
        }
        // Not magic — the u32 we just read is the object_length.
        AipsIoObjectHeader header;
        header.object_length = maybe_magic;
        header.object_type = read_string();
        header.object_version = read_u32();
        return header;
    }
    throw std::runtime_error("AipsIO: not enough data for object header");
}

} // namespace casacore_mini
