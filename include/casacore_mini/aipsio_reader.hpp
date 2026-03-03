// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

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

///
/// Decoded `AipsIO` object header fields.
///
///
///
///
///
/// Holds the three fields that prefix every casacore `AipsIO` object body:
/// the body length in bytes, the object type token (for example `Table`
/// or `RecordDesc`), and the object version number used for forward-
/// compatible deserialization.
///
/// Root-level objects are preceded by the four-byte magic
/// `0xBEBEBEBE`; nested objects omit it.  After reading a header with
/// `AipsIoReader::read_object_header` or
/// `AipsIoReader::read_nested_object_header`, the caller is positioned at the
/// first byte of the object body.
///
struct AipsIoObjectHeader {
    /// Object body length as stored in the stream.
    std::uint32_t object_length = 0;
    /// Object type token, for example `Table`.
    std::string object_type;
    /// Object version number.
    std::uint32_t object_version = 0;

    [[nodiscard]] bool operator==(const AipsIoObjectHeader& other) const = default;
};

///
/// Read primitive values from a canonical big-endian `AipsIO` byte span.
///
///
///
///
///
/// `AipsIoReader` wraps an immutable byte span and exposes a forward-only
/// cursor that decodes the big-endian, network-byte-order representation used
/// by casacore's `AipsIO` serialization layer.
///
/// All multi-byte scalars are decoded from big-endian wire order to native
/// byte order.  Strings use casacore's TypeIO framing: a 32-bit `uInt` length
/// followed by that many raw UTF-8 bytes (no null terminator).
///
/// Object headers are decoded by `read_object_header` (root level, with magic
/// prefix) and `read_nested_object_header` (sub-object, no magic prefix).
/// `read_object_header_auto` peeks at the next four bytes and selects the
/// appropriate variant automatically.
///
/// The reader does not support seeking backward or rewinding.  Position queries
/// (`position()`, `remaining()`, `empty()`) allow callers to validate framing
/// without modifying the cursor.
///
///
/// @par Example
/// @code{.cpp}
///   std::vector<uint8_t> raw = read_file("table.dat");
///   AipsIoReader reader(raw);
///   auto hdr = reader.read_object_header();       // consumes magic + header
///   auto row_count = reader.read_u64();           // next field in object body
/// @endcode
///
///
/// @par Motivation
/// casacore writes all persistent state through `AipsIO`, a custom big-endian
/// serialization format that pre-dates modern C++ serialization libraries.
/// This reader provides a zero-copy, exception-safe decoder that stays within
/// the immutable span without copying bytes into temporary buffers.
///
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
