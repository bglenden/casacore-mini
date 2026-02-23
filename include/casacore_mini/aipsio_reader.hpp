#pragma once

#include "casacore_mini/platform.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace casacore_mini {

/// Magic value written by casacore `AipsIO` object framing.
inline constexpr std::uint32_t kAipsIoMagic = 0xBEBEBEBEU;

/// Decoded `AipsIO` object header fields.
struct AipsIoObjectHeader {
    std::uint32_t object_length = 0;
    std::string object_type;
    std::uint32_t object_version = 0;

    [[nodiscard]] bool operator==(const AipsIoObjectHeader& other) const = default;
};

/// Read primitive values from a canonical big-endian `AipsIO` byte span.
///
/// This is an initial Phase 2 read-only scaffold for compatibility work.
class AipsIoReader {
  public:
    explicit AipsIoReader(std::span<const std::uint8_t> bytes);

    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::size_t remaining() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    /// Skip a fixed number of bytes.
    void skip(std::size_t byte_count);

    [[nodiscard]] std::uint8_t read_u8();
    [[nodiscard]] std::int16_t read_i16();
    [[nodiscard]] std::uint16_t read_u16();
    [[nodiscard]] std::int32_t read_i32();
    [[nodiscard]] std::uint32_t read_u32();
    [[nodiscard]] std::int64_t read_i64();
    [[nodiscard]] std::uint64_t read_u64();
    [[nodiscard]] float read_f32();
    [[nodiscard]] double read_f64();
    [[nodiscard]] std::complex<float> read_complex64();
    [[nodiscard]] std::complex<double> read_complex128();

    /// Read a TypeIO-style string (`uInt` length followed by raw bytes).
    [[nodiscard]] std::string read_string();

    /// Read an `AipsIO` object header.
    ///
    /// Header layout:
    /// - `uInt` magic (`0xBEBEBEBE`)
    /// - `uInt` object length
    /// - `String` object type
    /// - `uInt` object version
    [[nodiscard]] AipsIoObjectHeader read_object_header();

  private:
    [[nodiscard]] std::span<const std::uint8_t> read_bytes(std::size_t byte_count);

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
};

} // namespace casacore_mini
