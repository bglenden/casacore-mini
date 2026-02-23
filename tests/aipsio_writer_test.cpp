#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_writer.hpp"

#include <complex>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
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

bool test_primitive_round_trips() {
    casacore_mini::AipsIoWriter writer;
    writer.write_u8(0x7FU);
    writer.write_u16(0x1234U);
    writer.write_i16(static_cast<std::int16_t>(-2));
    writer.write_u32(0x01020304U);
    writer.write_i32(-123456789);
    writer.write_u64(0x0102030405060708ULL);
    writer.write_i64(-1234567890123456789LL);
    writer.write_f32(1.25F);
    writer.write_f64(-2.5);
    writer.write_complex64(std::complex<float>(3.5F, -4.25F));
    writer.write_complex128(std::complex<double>(5.5, -6.75));
    writer.write_string("mini");

    const auto& bytes = writer.bytes();
    casacore_mini::AipsIoReader reader(bytes);

    if (!expect_true(reader.read_u8() == 0x7FU, "u8 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u16() == 0x1234U, "u16 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i16() == -2, "i16 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u32() == 0x01020304U, "u32 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i32() == -123456789, "i32 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u64() == 0x0102030405060708ULL, "u64 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i64() == -1234567890123456789LL, "i64 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_f32() == 1.25F, "f32 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_f64() == -2.5, "f64 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_complex64() == std::complex<float>(3.5F, -4.25F),
                     "complex64 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_complex128() == std::complex<double>(5.5, -6.75),
                     "complex128 round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_string() == "mini", "string round-trip")) {
        return false;
    }
    return expect_true(reader.empty(), "reader should be empty after all primitive round-trips");
}

bool test_object_header_round_trip() {
    casacore_mini::AipsIoWriter writer;
    writer.write_object_header(42U, "TableDesc", 7U);
    writer.write_u32(0xDEADBEEFU);

    const auto& bytes = writer.bytes();
    casacore_mini::AipsIoReader reader(bytes);

    const auto header = reader.read_object_header();
    if (!expect_true(header.object_length == 42U, "object_length round-trip")) {
        return false;
    }
    if (!expect_true(header.object_type == "TableDesc", "object_type round-trip")) {
        return false;
    }
    if (!expect_true(header.object_version == 7U, "object_version round-trip")) {
        return false;
    }
    return expect_true(reader.read_u32() == 0xDEADBEEFU, "trailing payload round-trip");
}

bool test_write_matches_manual_big_endian() {
    casacore_mini::AipsIoWriter writer;
    writer.write_u32(0x01020304U);

    const auto& bytes = writer.bytes();
    if (!expect_true(bytes.size() == 4U, "u32 should produce 4 bytes")) {
        return false;
    }
    if (!expect_true(bytes[0] == 0x01U, "u32 byte 0")) {
        return false;
    }
    if (!expect_true(bytes[1] == 0x02U, "u32 byte 1")) {
        return false;
    }
    if (!expect_true(bytes[2] == 0x03U, "u32 byte 2")) {
        return false;
    }
    return expect_true(bytes[3] == 0x04U, "u32 byte 3");
}

bool test_deterministic_encoding() {
    auto build = []() {
        casacore_mini::AipsIoWriter writer;
        writer.write_object_header(100U, "Record", 3U);
        writer.write_i32(-42);
        writer.write_f64(3.14);
        writer.write_string("deterministic");
        writer.write_complex128(std::complex<double>(1.0, -1.0));
        return writer.take_bytes();
    };

    const auto first = build();
    const auto second = build();
    return expect_true(first == second, "AipsIO write encoding is not deterministic");
}

bool test_empty_string_round_trip() {
    casacore_mini::AipsIoWriter writer;
    writer.write_string("");

    const auto& bytes = writer.bytes();
    casacore_mini::AipsIoReader reader(bytes);

    if (!expect_true(reader.read_string().empty(), "empty string round-trip")) {
        return false;
    }
    return expect_true(reader.empty(), "reader should be empty after empty string");
}

bool test_take_bytes_clears_writer() {
    casacore_mini::AipsIoWriter writer;
    writer.write_u32(1U);

    if (!expect_true(writer.size() == 4U, "writer should have 4 bytes before take")) {
        return false;
    }

    const auto taken = writer.take_bytes();
    if (!expect_true(taken.size() == 4U, "taken bytes should have 4 bytes")) {
        return false;
    }
    return expect_true(writer.size() == 0U, "writer should be empty after take_bytes");
}

bool test_boundary_values() {
    casacore_mini::AipsIoWriter writer;
    writer.write_i16(std::numeric_limits<std::int16_t>::min());
    writer.write_i16(std::numeric_limits<std::int16_t>::max());
    writer.write_u16(std::numeric_limits<std::uint16_t>::max());
    writer.write_i32(std::numeric_limits<std::int32_t>::min());
    writer.write_i32(std::numeric_limits<std::int32_t>::max());
    writer.write_u32(std::numeric_limits<std::uint32_t>::max());
    writer.write_i64(std::numeric_limits<std::int64_t>::min());
    writer.write_i64(std::numeric_limits<std::int64_t>::max());
    writer.write_u64(std::numeric_limits<std::uint64_t>::max());

    const auto& bytes = writer.bytes();
    casacore_mini::AipsIoReader reader(bytes);

    if (!expect_true(reader.read_i16() == std::numeric_limits<std::int16_t>::min(),
                     "i16 min round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i16() == std::numeric_limits<std::int16_t>::max(),
                     "i16 max round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u16() == std::numeric_limits<std::uint16_t>::max(),
                     "u16 max round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i32() == std::numeric_limits<std::int32_t>::min(),
                     "i32 min round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i32() == std::numeric_limits<std::int32_t>::max(),
                     "i32 max round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u32() == std::numeric_limits<std::uint32_t>::max(),
                     "u32 max round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i64() == std::numeric_limits<std::int64_t>::min(),
                     "i64 min round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_i64() == std::numeric_limits<std::int64_t>::max(),
                     "i64 max round-trip")) {
        return false;
    }
    if (!expect_true(reader.read_u64() == std::numeric_limits<std::uint64_t>::max(),
                     "u64 max round-trip")) {
        return false;
    }
    return expect_true(reader.empty(), "reader should be empty after boundary values");
}

} // namespace

int main() noexcept {
    try {
        if (!test_primitive_round_trips()) {
            return 1;
        }
        if (!test_object_header_round_trip()) {
            return 1;
        }
        if (!test_write_matches_manual_big_endian()) {
            return 1;
        }
        if (!test_deterministic_encoding()) {
            return 1;
        }
        if (!test_empty_string_round_trip()) {
            return 1;
        }
        if (!test_take_bytes_clears_writer()) {
            return 1;
        }
        if (!test_boundary_values()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "aipsio_writer_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
