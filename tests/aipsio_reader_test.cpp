// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/aipsio_reader.hpp"

#include <bit>
#include <complex>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool expect_true(const bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

void append_u16(std::vector<std::uint8_t>& bytes, const std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void append_u64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        const auto shift = static_cast<unsigned int>((sizeof(value) - 1U - index) * 8U);
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void append_f32(std::vector<std::uint8_t>& bytes, const float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void append_f64(std::vector<std::uint8_t>& bytes, const double value) {
    append_u64(bytes, std::bit_cast<std::uint64_t>(value));
}

void append_string(std::vector<std::uint8_t>& bytes, const std::string& value) {
    append_u32(bytes, static_cast<std::uint32_t>(value.size()));
    for (const char ch : value) {
        bytes.push_back(static_cast<std::uint8_t>(ch));
    }
}

bool test_reads_primitive_values() {
    std::vector<std::uint8_t> bytes;

    bytes.push_back(0x7FU);
    append_u16(bytes, 0x1234U);
    append_u16(bytes, std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(-2)));
    append_u32(bytes, 0x01020304U);
    append_u32(bytes, std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(-123456789)));
    append_u64(bytes, 0x0102030405060708ULL);
    append_u64(bytes,
               std::bit_cast<std::uint64_t>(static_cast<std::int64_t>(-1234567890123456789LL)));
    append_f32(bytes, 1.25F);
    append_f64(bytes, -2.5);
    append_f32(bytes, 3.5F);
    append_f32(bytes, -4.25F);
    append_f64(bytes, 5.5);
    append_f64(bytes, -6.75);
    append_string(bytes, "mini");

    casacore_mini::AipsIoReader reader(bytes);
    if (!expect_true(reader.read_u8() == 0x7FU, "read_u8 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_u16() == 0x1234U, "read_u16 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_i16() == -2, "read_i16 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_u32() == 0x01020304U, "read_u32 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_i32() == -123456789, "read_i32 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_u64() == 0x0102030405060708ULL, "read_u64 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_i64() == -1234567890123456789LL, "read_i64 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_f32() == 1.25F, "read_f32 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_f64() == -2.5, "read_f64 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_complex64() == std::complex<float>(3.5F, -4.25F),
                     "read_complex64 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_complex128() == std::complex<double>(5.5, -6.75),
                     "read_complex128 mismatch")) {
        return false;
    }
    if (!expect_true(reader.read_string() == "mini", "read_string mismatch")) {
        return false;
    }
    return expect_true(reader.empty(), "reader should be empty after primitive reads");
}

bool test_reads_object_header() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, casacore_mini::kAipsIoMagic);
    append_u32(bytes, 42U);
    append_string(bytes, "TableDesc");
    append_u32(bytes, 7U);
    append_u32(bytes, 0xDEADBEEFU);

    casacore_mini::AipsIoReader reader(bytes);
    const auto header = reader.read_object_header();
    if (!expect_true(header.object_length == 42U, "object_length mismatch")) {
        return false;
    }
    if (!expect_true(header.object_type == "TableDesc", "object_type mismatch")) {
        return false;
    }
    if (!expect_true(header.object_version == 7U, "object_version mismatch")) {
        return false;
    }
    if (!expect_true(reader.remaining() == sizeof(std::uint32_t),
                     "remaining bytes after object header mismatch")) {
        return false;
    }
    return expect_true(reader.read_u32() == 0xDEADBEEFU, "payload value mismatch");
}

bool test_rejects_invalid_magic() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, 0U);
    append_u32(bytes, 1U);
    append_string(bytes, "x");
    append_u32(bytes, 1U);

    try {
        casacore_mini::AipsIoReader reader(bytes);
        static_cast<void>(reader.read_object_header());
    } catch (const std::exception&) {
        return true;
    }

    std::cerr << "reader accepted invalid AipsIO magic\n";
    return false;
}

bool test_rejects_truncated_string() {
    std::vector<std::uint8_t> bytes;
    append_u32(bytes, casacore_mini::kAipsIoMagic);
    append_u32(bytes, 10U);
    append_u32(bytes, 5U);
    bytes.push_back(static_cast<std::uint8_t>('a'));
    bytes.push_back(static_cast<std::uint8_t>('b'));
    append_u32(bytes, 1U);

    try {
        casacore_mini::AipsIoReader reader(bytes);
        static_cast<void>(reader.read_object_header());
    } catch (const std::exception&) {
        return true;
    }

    std::cerr << "reader accepted truncated AipsIO string\n";
    return false;
}

} // namespace

int main() noexcept {
    try {
        if (!test_reads_primitive_values()) {
            return 1;
        }
        if (!test_reads_object_header()) {
            return 1;
        }
        if (!test_rejects_invalid_magic()) {
            return 1;
        }
        if (!test_rejects_truncated_string()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "aipsio_reader_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
