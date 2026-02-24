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

/// Encode primitive values into a canonical big-endian `AipsIO` byte buffer.
///
/// This is the write-path counterpart to `AipsIoReader`. All multi-byte values
/// are written in big-endian byte order matching casacore canonical format.
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

    /// Write an `AipsIO` object header.
    ///
    /// Header layout:
    /// - `uInt` magic (`0xBEBEBEBE`)
    /// - `uInt` object length (caller-supplied)
    /// - `String` object type
    /// - `uInt` object version
    void write_object_header(std::uint32_t object_length, std::string_view object_type,
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
