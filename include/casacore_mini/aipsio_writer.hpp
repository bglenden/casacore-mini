// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "casacore_mini/platform.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Write-only encoder for canonical big-endian casacore `AipsIO` payloads.

/// <summary>
/// Encode primitive values into a canonical big-endian `AipsIO` byte buffer.
/// </summary>
///
/// <use visibility=local/>
///
/// <synopsis>
/// `AipsIoWriter` is the write-path counterpart to `AipsIoReader`.  It
/// appends big-endian encoded primitives to an internal `std::vector<uint8_t>`
/// buffer that grows on demand.  When serialization is complete the caller
/// either calls `take_bytes()` to move the buffer out, or `bytes()` to inspect
/// it in place.
///
/// Object headers are written by `write_object_header` (root level, includes
/// the `0xBEBEBEBE` magic prefix) and `write_nested_object_header` (sub-object,
/// no magic prefix).  Because the object body length is not known until after
/// the body has been written, callers typically:
///
/// 1. Record the current `size()` as the patch position.
/// 2. Write a placeholder length with `write_u32(0)` inside the header call.
/// 3. Write the body.
/// 4. Call `patch_u32(patch_position, actual_length)` to back-fill the length.
///
/// All multi-byte scalars are encoded in big-endian byte order.  Strings use
/// casacore's TypeIO framing: a 32-bit `uInt` length prefix followed by the
/// raw bytes of the string (no null terminator).
/// </synopsis>
///
/// <example>
/// <srcblock>
///   AipsIoWriter w;
///   std::size_t len_pos = w.size();
///   w.write_object_header(0, "Table", 2);   // placeholder length = 0
///   w.write_u64(row_count);
///   // ... write remaining body ...
///   std::uint32_t body = static_cast<uint32_t>(w.size() - len_pos - 8);
///   w.patch_u32(len_pos + 4, body);         // back-patch length field
///   auto bytes = w.take_bytes();
/// </srcblock>
/// </example>
///
/// <motivation>
/// casacore's `AipsIO` uses a push-style API where the object-length field must
/// be filled in after the body is serialized.  `patch_u32` makes this pattern
/// safe without requiring two-pass serialization or temporary buffers.
/// </motivation>
class AipsIoWriter {
  public:
    /// Create a writer that appends to a byte buffer.
    AipsIoWriter();

    /// Current size of the written buffer.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Move the accumulated bytes out of the writer.
    [[nodiscard]] std::vector<std::uint8_t> take_bytes();

    /// Immutable view of the accumulated bytes.
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept;

    /// Write one unsigned byte.
    void write_u8(std::uint8_t value);
    /// Write signed 16-bit integer in big-endian order.
    void write_i16(std::int16_t value);
    /// Write unsigned 16-bit integer in big-endian order.
    void write_u16(std::uint16_t value);
    /// Write signed 32-bit integer in big-endian order.
    void write_i32(std::int32_t value);
    /// Write unsigned 32-bit integer in big-endian order.
    void write_u32(std::uint32_t value);
    /// Write signed 64-bit integer in big-endian order.
    void write_i64(std::int64_t value);
    /// Write unsigned 64-bit integer in big-endian order.
    void write_u64(std::uint64_t value);
    /// Write IEEE754 `float` in big-endian order.
    void write_f32(float value);
    /// Write IEEE754 `double` in big-endian order.
    void write_f64(double value);
    /// Write `complex<float>` as two adjacent `f32` values (real, imag).
    void write_complex64(std::complex<float> value);
    /// Write `complex<double>` as two adjacent `f64` values (real, imag).
    void write_complex128(std::complex<double> value);

    /// Write a TypeIO-style string (`uInt` length followed by raw bytes).
    void write_string(std::string_view value);

    /// Write an `AipsIO` root-level object header (with magic prefix).
    ///
    /// Header layout:
    /// - `uInt` magic (`0xBEBEBEBE`)
    /// - `uInt` object length (caller-supplied)
    /// - `String` object type
    /// - `uInt` object version
    void write_object_header(std::uint32_t object_length, std::string_view object_type,
                             std::uint32_t object_version);

    /// Write a nested AipsIO sub-object header (no magic prefix).
    ///
    /// Header layout:
    /// - `uInt` object length (caller-supplied)
    /// - `String` object type
    /// - `uInt` object version
    ///
    /// Use this for objects nested inside a root-level object, matching
    /// casacore's AipsIO::putstart behavior at level > 0.
    void write_nested_object_header(std::uint32_t object_length, std::string_view object_type,
                                    std::uint32_t object_version);

    /// Overwrite 4 bytes at @p position with big-endian @p value.
    ///
    /// Used to back-patch object-length fields after the body is written.
    ///
    /// @throws std::out_of_range if @p position + 4 exceeds current buffer size.
    void patch_u32(std::size_t position, std::uint32_t value);

  private:
    void write_bytes(const std::uint8_t* data, std::size_t count);

    std::vector<std::uint8_t> buffer_;
};

} // namespace casacore_mini
