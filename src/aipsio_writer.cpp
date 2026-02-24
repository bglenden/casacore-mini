#include "casacore_mini/aipsio_writer.hpp"

#include "casacore_mini/aipsio_reader.hpp"

#include <bit>
#include <stdexcept>
#include <type_traits>

namespace casacore_mini {
namespace {

template <typename unsigned_t>
void encode_unsigned_be(std::vector<std::uint8_t>& buffer, const unsigned_t value) {
    static_assert(std::is_unsigned_v<unsigned_t>);
    for (std::size_t index = 0; index < sizeof(unsigned_t); ++index) {
        const auto shift = static_cast<unsigned int>((sizeof(unsigned_t) - 1U - index) * 8U);
        buffer.push_back(
            static_cast<std::uint8_t>((value >> shift) & static_cast<unsigned_t>(0xFFU)));
    }
}

} // namespace

AipsIoWriter::AipsIoWriter() = default;

std::size_t AipsIoWriter::size() const noexcept {
    return buffer_.size();
}

std::vector<std::uint8_t> AipsIoWriter::take_bytes() {
    auto result = std::move(buffer_);
    buffer_.clear();
    return result;
}

const std::vector<std::uint8_t>& AipsIoWriter::bytes() const noexcept {
    return buffer_;
}

void AipsIoWriter::write_bytes(const std::uint8_t* data, const std::size_t count) {
    buffer_.insert(buffer_.end(), data, data + count);
}

void AipsIoWriter::write_u8(const std::uint8_t value) {
    buffer_.push_back(value);
}

void AipsIoWriter::write_i16(const std::int16_t value) {
    write_u16(std::bit_cast<std::uint16_t>(value));
}

void AipsIoWriter::write_u16(const std::uint16_t value) {
    encode_unsigned_be(buffer_, value);
}

void AipsIoWriter::write_i32(const std::int32_t value) {
    write_u32(std::bit_cast<std::uint32_t>(value));
}

void AipsIoWriter::write_u32(const std::uint32_t value) {
    encode_unsigned_be(buffer_, value);
}

void AipsIoWriter::write_i64(const std::int64_t value) {
    write_u64(std::bit_cast<std::uint64_t>(value));
}

void AipsIoWriter::write_u64(const std::uint64_t value) {
    encode_unsigned_be(buffer_, value);
}

void AipsIoWriter::write_f32(const float value) {
    write_u32(std::bit_cast<std::uint32_t>(value));
}

void AipsIoWriter::write_f64(const double value) {
    write_u64(std::bit_cast<std::uint64_t>(value));
}

void AipsIoWriter::write_complex64(const std::complex<float> value) {
    write_f32(value.real());
    write_f32(value.imag());
}

void AipsIoWriter::write_complex128(const std::complex<double> value) {
    write_f64(value.real());
    write_f64(value.imag());
}

void AipsIoWriter::write_string(const std::string_view value) {
    write_u32(static_cast<std::uint32_t>(value.size()));
    if (!value.empty()) {
        write_bytes(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
    }
}

void AipsIoWriter::write_object_header(const std::uint32_t object_length,
                                       const std::string_view object_type,
                                       const std::uint32_t object_version) {
    write_u32(kAipsIoMagic);
    write_u32(object_length);
    write_string(object_type);
    write_u32(object_version);
}

void AipsIoWriter::write_nested_object_header(const std::uint32_t object_length,
                                              const std::string_view object_type,
                                              const std::uint32_t object_version) {
    write_u32(object_length);
    write_string(object_type);
    write_u32(object_version);
}

void AipsIoWriter::patch_u32(const std::size_t position, const std::uint32_t value) {
    if (position + sizeof(std::uint32_t) > buffer_.size()) {
        throw std::out_of_range("patch_u32 position exceeds buffer size");
    }
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        const auto shift = static_cast<unsigned int>((sizeof(std::uint32_t) - 1U - index) * 8U);
        buffer_[position + index] = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
    }
}

} // namespace casacore_mini
