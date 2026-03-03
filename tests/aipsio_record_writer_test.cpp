// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/aipsio_record_reader.hpp"
#include "casacore_mini/aipsio_record_writer.hpp"
#include "casacore_mini/aipsio_writer.hpp"
#include "casacore_mini/record.hpp"

#include <complex>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
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

/// Write a Record, read it back, return true if round-trip preserves equality.
bool round_trip(const casacore_mini::Record& original) {
    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, original);
    auto bytes = writer.take_bytes();
    casacore_mini::AipsIoReader reader(bytes);
    const auto decoded = casacore_mini::read_aipsio_record(reader);
    return original == decoded;
}

bool test_scalar_round_trip() {
    casacore_mini::Record record;
    record.set("flag", casacore_mini::RecordValue(true));
    record.set("i16", casacore_mini::RecordValue(std::int16_t{-42}));
    record.set("u16", casacore_mini::RecordValue(std::uint16_t{1000}));
    record.set("i32", casacore_mini::RecordValue(std::int32_t{-100000}));
    record.set("u32", casacore_mini::RecordValue(std::uint32_t{200000}));
    record.set("i64", casacore_mini::RecordValue(std::int64_t{9000000000LL}));
    record.set("f32", casacore_mini::RecordValue(1.5F));
    record.set("f64", casacore_mini::RecordValue(3.14));
    record.set("label", casacore_mini::RecordValue("hello"));

    return expect_true(round_trip(record), "scalar round-trip failed");
}

bool test_array_round_trip() {
    casacore_mini::Record record;

    casacore_mini::RecordValue::double_array darr;
    darr.shape = {2, 3};
    darr.elements = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};
    record.set("doubles", casacore_mini::RecordValue(std::move(darr)));

    casacore_mini::RecordValue::string_array sarr;
    sarr.shape = {3};
    sarr.elements = {"alpha", "beta", "gamma"};
    record.set("strings", casacore_mini::RecordValue(std::move(sarr)));

    casacore_mini::RecordValue::int32_array iarr;
    iarr.shape = {2, 2};
    iarr.elements = {10, 20, 30, 40};
    record.set("ints", casacore_mini::RecordValue(std::move(iarr)));

    return expect_true(round_trip(record), "array round-trip failed");
}

bool test_nested_record_round_trip() {
    casacore_mini::Record inner;
    inner.set("pi", casacore_mini::RecordValue(3.14159));
    inner.set("name", casacore_mini::RecordValue("test"));

    casacore_mini::Record outer;
    outer.set("child", casacore_mini::RecordValue::from_record(std::move(inner)));
    outer.set("value", casacore_mini::RecordValue(std::int32_t{42}));

    return expect_true(round_trip(outer), "nested record round-trip failed");
}

bool test_complex_round_trip() {
    casacore_mini::Record record;
    record.set("cf", casacore_mini::RecordValue(std::complex<float>(1.0F, -2.0F)));
    record.set("cd", casacore_mini::RecordValue(std::complex<double>(3.0, -4.0)));

    casacore_mini::RecordValue::complex64_array carr;
    carr.shape = {2};
    carr.elements = {{1.0F, 2.0F}, {3.0F, 4.0F}};
    record.set("cf_arr", casacore_mini::RecordValue(std::move(carr)));

    casacore_mini::RecordValue::complex128_array darr;
    darr.shape = {2};
    darr.elements = {{5.0, 6.0}, {7.0, 8.0}};
    record.set("cd_arr", casacore_mini::RecordValue(std::move(darr)));

    return expect_true(round_trip(record), "complex round-trip failed");
}

bool test_empty_record() {
    casacore_mini::Record record;
    return expect_true(round_trip(record), "empty record round-trip failed");
}

bool test_deterministic_output() {
    casacore_mini::Record record;
    record.set("x", casacore_mini::RecordValue(1.0));
    record.set("y", casacore_mini::RecordValue("hello"));

    casacore_mini::AipsIoWriter writer1;
    casacore_mini::write_aipsio_record(writer1, record);
    auto bytes1 = writer1.take_bytes();

    casacore_mini::AipsIoWriter writer2;
    casacore_mini::write_aipsio_record(writer2, record);
    auto bytes2 = writer2.take_bytes();

    return expect_true(bytes1 == bytes2, "deterministic output failed");
}

bool test_object_lengths_valid() {
    casacore_mini::Record record;
    record.set("value", casacore_mini::RecordValue(42.0));

    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, record);
    auto bytes = writer.take_bytes();

    // Parse the Record object header and verify the length field is correct.
    casacore_mini::AipsIoReader reader(bytes);
    const auto header = reader.read_object_header();
    if (!expect_true(header.object_type == "Record", "root should be Record")) {
        return false;
    }

    // casacore stores object_length as the full object byte count excluding
    // only the leading magic word.
    const auto expected_length = static_cast<std::uint32_t>(bytes.size() - 4U);
    return expect_true(header.object_length == expected_length, "object length mismatch");
}

[[nodiscard]] std::size_t count_magic_markers(const std::vector<std::uint8_t>& bytes) {
    std::size_t count = 0;
    for (std::size_t index = 0; index + 3U < bytes.size(); ++index) {
        if (bytes[index] == 0xBEU && bytes[index + 1U] == 0xBEU && bytes[index + 2U] == 0xBEU &&
            bytes[index + 3U] == 0xBEU) {
            ++count;
        }
    }
    return count;
}

bool test_only_root_object_has_magic() {
    casacore_mini::Record child;
    child.set("name", casacore_mini::RecordValue("nested"));

    casacore_mini::Record record;
    record.set("child", casacore_mini::RecordValue::from_record(std::move(child)));
    record.set("value", casacore_mini::RecordValue(std::int32_t{42}));

    casacore_mini::RecordValue::double_array arr;
    arr.shape = {2U};
    arr.elements = {1.0, 2.0};
    record.set("arr", casacore_mini::RecordValue(std::move(arr)));

    casacore_mini::AipsIoWriter writer;
    casacore_mini::write_aipsio_record(writer, record);
    const auto bytes = writer.take_bytes();
    return expect_true(count_magic_markers(bytes) == 1U,
                       "writer should emit only one AipsIO magic marker");
}

bool test_rejects_uint64_scalar() {
    casacore_mini::Record record;
    record.set("unsupported_u64", casacore_mini::RecordValue(std::uint64_t{42}));

    casacore_mini::AipsIoWriter writer;
    try {
        casacore_mini::write_aipsio_record(writer, record);
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << "writer accepted unsupported uint64 scalar\n";
    return false;
}

bool test_rejects_uint64_array() {
    casacore_mini::Record record;
    casacore_mini::RecordValue::uint64_array values;
    values.shape = {1U};
    values.elements = {42U};
    record.set("unsupported_u64_array", casacore_mini::RecordValue(std::move(values)));

    casacore_mini::AipsIoWriter writer;
    try {
        casacore_mini::write_aipsio_record(writer, record);
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << "writer accepted unsupported uint64 array\n";
    return false;
}

bool test_rejects_out_of_range_dimension() {
    casacore_mini::Record record;
    casacore_mini::RecordValue::int32_array values;
    const auto too_large_dim =
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1ULL;
    values.shape = {0U, too_large_dim};
    values.elements = {};
    record.set("bad_dim", casacore_mini::RecordValue(std::move(values)));

    casacore_mini::AipsIoWriter writer;
    try {
        casacore_mini::write_aipsio_record(writer, record);
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << "writer accepted out-of-range array dimension\n";
    return false;
}

} // namespace

int main() noexcept {
    try {
        if (!test_scalar_round_trip()) {
            return 1;
        }
        if (!test_array_round_trip()) {
            return 1;
        }
        if (!test_nested_record_round_trip()) {
            return 1;
        }
        if (!test_complex_round_trip()) {
            return 1;
        }
        if (!test_empty_record()) {
            return 1;
        }
        if (!test_deterministic_output()) {
            return 1;
        }
        if (!test_object_lengths_valid()) {
            return 1;
        }
        if (!test_only_root_object_has_magic()) {
            return 1;
        }
        if (!test_rejects_uint64_scalar()) {
            return 1;
        }
        if (!test_rejects_uint64_array()) {
            return 1;
        }
        if (!test_rejects_out_of_range_dimension()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "aipsio_record_writer_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
