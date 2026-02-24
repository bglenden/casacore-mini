#pragma once

#include "casacore_mini/platform.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace casacore_mini {

/// @file
/// @brief Read-only decoder for canonical big-endian casacore `AipsIO` payloads.

/// Magic value written by casacore `AipsIO` object framing.
inline constexpr std::uint32_t kAipsIoMagic = 0xBEBEBEBEU;

/// Decoded `AipsIO` object header fields.
struct AipsIoObjectHeader {
    /// Object body length as stored in the stream.
    std::uint32_t object_length = 0;
    /// Object type token, for example `Table`.
    std::string object_type;
    /// Object version number.
    std::uint32_t object_version = 0;

    [[nodiscard]] bool operator==(const AipsIoObjectHeader& other) const = default;
};

/// Read primitive values from a canonical big-endian `AipsIO` byte span.
///
/// This is an initial Phase 2 read-only scaffold for compatibility work.
class AipsIoReader {
  public:
    /// Create a reader over immutable bytes.
    explicit AipsIoReader(std::span<const std::uint8_t> bytes);

    /// Current cursor offset from the start of the span.
    [[nodiscard]] std::size_t position() const noexcept;
    /// Number of unread bytes remaining.
    [[nodiscard]] std::size_t remaining() const noexcept;
    /// True when no bytes remain.
    [[nodiscard]] bool empty() const noexcept;

    /// Skip a fixed number of bytes.
    /// @throws std::runtime_error if `byte_count` exceeds remaining bytes.
    void skip(std::size_t byte_count);

    /// Read one unsigned byte.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::uint8_t read_u8();
    /// Read signed 16-bit integer (big-endian two's-complement bit pattern).
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::int16_t read_i16();
    /// Read unsigned 16-bit integer in big-endian order.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::uint16_t read_u16();
    /// Read signed 32-bit integer (big-endian two's-complement bit pattern).
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::int32_t read_i32();
    /// Read unsigned 32-bit integer in big-endian order.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::uint32_t read_u32();
    /// Read signed 64-bit integer (big-endian two's-complement bit pattern).
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::int64_t read_i64();
    /// Read unsigned 64-bit integer in big-endian order.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::uint64_t read_u64();
    /// Read IEEE754 `float` from big-endian bytes.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] float read_f32();
    /// Read IEEE754 `double` from big-endian bytes.
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] double read_f64();
    /// Read `complex<float>` as two adjacent `f32` values (real, imag).
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::complex<float> read_complex64();
    /// Read `complex<double>` as two adjacent `f64` values (real, imag).
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] std::complex<double> read_complex128();

    /// Read a TypeIO-style string (`uInt` length followed by raw bytes).
    /// @throws std::runtime_error if size does not fit host `size_t` or input is truncated.
    [[nodiscard]] std::string read_string();

    /// Read a root-level `AipsIO` object header (expects magic prefix).
    ///
    /// Header layout:
    /// - `uInt` magic (`0xBEBEBEBE`)
    /// - `uInt` object length
    /// - `String` object type
    /// - `uInt` object version
    ///
    /// @throws std::runtime_error if magic is invalid or input is truncated.
    [[nodiscard]] AipsIoObjectHeader read_object_header();

    /// Read a nested `AipsIO` sub-object header (no magic prefix).
    ///
    /// Header layout:
    /// - `uInt` object length
    /// - `String` object type
    /// - `uInt` object version
    ///
    /// @throws std::runtime_error on truncated input.
    [[nodiscard]] AipsIoObjectHeader read_nested_object_header();

    /// Read an object header, auto-detecting whether magic is present.
    /// Peeks at the next 4 bytes: if they match kAipsIoMagic, reads a
    /// root-level header; otherwise reads a nested header.
    [[nodiscard]] AipsIoObjectHeader read_object_header_auto();

  private:
    [[nodiscard]] std::span<const std::uint8_t> read_bytes(std::size_t byte_count);

    std::span<const std::uint8_t> bytes_;
    std::size_t offset_ = 0;
};

} // namespace casacore_mini
