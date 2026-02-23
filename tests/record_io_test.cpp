#include "casacore_mini/record.hpp"
#include "casacore_mini/record_io.hpp"

#include <bit>
#include <complex>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

bool expect_true(const bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

template <typename value_t>
bool expect_field_type(const casacore_mini::Record& record, const std::string_view key) {
    const auto* found = record.find(key);
    if (!expect_true(found != nullptr, "field missing after round-trip")) {
        return false;
    }
    return expect_true(std::holds_alternative<value_t>(found->storage()),
                       "field changed type after round-trip");
}

casacore_mini::Record build_scalar_record() {
    casacore_mini::Record value;
    value.set("flag", casacore_mini::RecordValue(true));
    value.set("i16", casacore_mini::RecordValue(std::int16_t(-7)));
    value.set("u16", casacore_mini::RecordValue(std::uint16_t(7)));
    value.set("i32", casacore_mini::RecordValue(std::int32_t(-70000)));
    value.set("u32", casacore_mini::RecordValue(std::uint32_t(70000)));
    value.set("i64", casacore_mini::RecordValue(std::int64_t(-9000000000LL)));
    value.set("u64", casacore_mini::RecordValue(std::uint64_t(9000000000ULL)));
    value.set("f32", casacore_mini::RecordValue(1.5F));
    value.set("f64", casacore_mini::RecordValue(1.5));
    value.set("label", casacore_mini::RecordValue("mini"));
    value.set("phasorf", casacore_mini::RecordValue(std::complex<float>(1.0F, -2.0F)));
    value.set("phasor", casacore_mini::RecordValue(std::complex<double>(1.0, -2.0)));
    return value;
}

casacore_mini::Record build_array_record() {
    casacore_mini::Record value;

    value.set("arr_i16",
              casacore_mini::RecordValue(casacore_mini::RecordValue::int16_array{{3}, {-1, 0, 1}}));
    value.set("arr_u16",
              casacore_mini::RecordValue(casacore_mini::RecordValue::uint16_array{{3}, {1, 2, 3}}));
    value.set("arr_i32", casacore_mini::RecordValue(casacore_mini::RecordValue::int32_array{
                             {2, 3}, {10, 11, 20, 21, 30, 31}}));
    value.set("arr_u32", casacore_mini::RecordValue(
                             casacore_mini::RecordValue::uint32_array{{2, 2}, {5, 6, 7, 8}}));
    value.set("arr_i64",
              casacore_mini::RecordValue(casacore_mini::RecordValue::int64_array{{2}, {-4, 9}}));
    value.set("arr_u64",
              casacore_mini::RecordValue(casacore_mini::RecordValue::uint64_array{{2}, {10, 20}}));
    value.set("arr_f32", casacore_mini::RecordValue(casacore_mini::RecordValue::float_array{
                             {2, 2}, {1.5F, 2.5F, 3.5F, 4.5F}}));
    value.set("arr_f64", casacore_mini::RecordValue(casacore_mini::RecordValue::double_array{
                             {2, 2, 2}, {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0}}));
    value.set("arr_str", casacore_mini::RecordValue(casacore_mini::RecordValue::string_array{
                             {2, 2}, {"a00", "a10", "a01", "a11"}}));
    value.set("arr_c64", casacore_mini::RecordValue(casacore_mini::RecordValue::complex64_array{
                             {2, 2},
                             {std::complex<float>(1.0F, 2.0F), std::complex<float>(3.0F, 4.0F),
                              std::complex<float>(5.0F, 6.0F), std::complex<float>(7.0F, 8.0F)}}));
    value.set("arr_c128", casacore_mini::RecordValue(casacore_mini::RecordValue::complex128_array{
                              {3},
                              {std::complex<double>(-1.0, 1.0), std::complex<double>(2.0, -2.0),
                               std::complex<double>(3.0, 0.5)}}));

    return value;
}

casacore_mini::Record build_nested_record() {
    casacore_mini::Record child;
    child.set("pi", casacore_mini::RecordValue(3.141592653589793));
    child.set("samples", casacore_mini::RecordValue(casacore_mini::RecordValue::float_array{
                             {2, 2}, {10.0F, 20.0F, 30.0F, 40.0F}}));

    casacore_mini::RecordList list;
    list.elements.push_back(casacore_mini::RecordValue(std::int64_t(1)));
    list.elements.push_back(casacore_mini::RecordValue("two"));
    list.elements.push_back(casacore_mini::RecordValue::from_record(child));

    casacore_mini::Record root;
    root.set("child", casacore_mini::RecordValue::from_record(child));
    root.set("items", casacore_mini::RecordValue::from_list(std::move(list)));
    return root;
}

bool test_scalar_round_trip() {
    const auto expected = build_scalar_record();
    const auto payload = casacore_mini::serialize_record_binary(expected);
    const auto actual = casacore_mini::deserialize_record_binary(payload);
    return expect_true(actual == expected, "scalar Record round-trip failed");
}

bool test_scalar_type_matrix_preserved() {
    const auto value = build_scalar_record();
    const auto payload = casacore_mini::serialize_record_binary(value);
    const auto round_trip = casacore_mini::deserialize_record_binary(payload);

    if (!expect_field_type<bool>(round_trip, "flag")) {
        return false;
    }
    if (!expect_field_type<std::int16_t>(round_trip, "i16")) {
        return false;
    }
    if (!expect_field_type<std::uint16_t>(round_trip, "u16")) {
        return false;
    }
    if (!expect_field_type<std::int32_t>(round_trip, "i32")) {
        return false;
    }
    if (!expect_field_type<std::uint32_t>(round_trip, "u32")) {
        return false;
    }
    if (!expect_field_type<std::int64_t>(round_trip, "i64")) {
        return false;
    }
    if (!expect_field_type<std::uint64_t>(round_trip, "u64")) {
        return false;
    }
    if (!expect_field_type<float>(round_trip, "f32")) {
        return false;
    }
    if (!expect_field_type<double>(round_trip, "f64")) {
        return false;
    }
    if (!expect_field_type<std::string>(round_trip, "label")) {
        return false;
    }
    if (!expect_field_type<std::complex<float>>(round_trip, "phasorf")) {
        return false;
    }
    return expect_field_type<std::complex<double>>(round_trip, "phasor");
}

bool test_array_round_trip() {
    const auto expected = build_array_record();
    const auto payload = casacore_mini::serialize_record_binary(expected);
    const auto actual = casacore_mini::deserialize_record_binary(payload);
    return expect_true(actual == expected, "array Record round-trip failed");
}

bool test_array_type_matrix_preserved() {
    const auto value = build_array_record();
    const auto payload = casacore_mini::serialize_record_binary(value);
    const auto round_trip = casacore_mini::deserialize_record_binary(payload);

    if (!expect_field_type<casacore_mini::RecordValue::int16_array>(round_trip, "arr_i16")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::uint16_array>(round_trip, "arr_u16")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::int32_array>(round_trip, "arr_i32")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::uint32_array>(round_trip, "arr_u32")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::int64_array>(round_trip, "arr_i64")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::uint64_array>(round_trip, "arr_u64")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::float_array>(round_trip, "arr_f32")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::double_array>(round_trip, "arr_f64")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::string_array>(round_trip, "arr_str")) {
        return false;
    }
    if (!expect_field_type<casacore_mini::RecordValue::complex64_array>(round_trip, "arr_c64")) {
        return false;
    }
    return expect_field_type<casacore_mini::RecordValue::complex128_array>(round_trip, "arr_c128");
}

bool test_nested_round_trip() {
    const auto expected = build_nested_record();
    const auto payload = casacore_mini::serialize_record_binary(expected);
    const auto actual = casacore_mini::deserialize_record_binary(payload);
    return expect_true(actual == expected, "nested Record round-trip failed");
}

bool test_deterministic_encoding() {
    const auto value = build_array_record();
    const auto first = casacore_mini::serialize_record_binary(value);
    const auto second = casacore_mini::serialize_record_binary(value);
    return expect_true(first == second, "binary Record encoding is not deterministic");
}

bool test_rejects_invalid_magic() {
    try {
        const std::vector<std::uint8_t> invalid{0U, 1U, 2U, 3U, 4U, 5U};
        static_cast<void>(casacore_mini::deserialize_record_binary(invalid));
    } catch (const std::exception&) {
        return true;
    }

    std::cerr << "deserializer accepted invalid magic header\n";
    return false;
}

bool test_set_replaces_existing_key() {
    casacore_mini::Record value;
    value.set("x", casacore_mini::RecordValue(std::int64_t(1)));
    value.set("x", casacore_mini::RecordValue(std::int64_t(2)));

    if (!expect_true(value.size() == 1U, "set() did not replace existing key")) {
        return false;
    }

    const auto* found = value.find("x");
    if (!expect_true(found != nullptr, "find() could not locate existing key")) {
        return false;
    }

    const auto payload = casacore_mini::serialize_record_binary(value);
    const auto round_trip = casacore_mini::deserialize_record_binary(payload);
    return expect_true(round_trip == value,
                       "replacement semantics were not preserved by round-trip");
}

bool test_rejects_invalid_array_shape() {
    try {
        const casacore_mini::RecordValue::int32_array invalid{{2, 2}, {1, 2, 3}};
        const auto value = casacore_mini::RecordValue(invalid);
        static_cast<void>(value);
    } catch (const std::invalid_argument&) {
        return true;
    }
    std::cerr << "RecordValue accepted mismatched array shape\n";
    return false;
}

void append_u8(std::vector<std::uint8_t>& bytes, const std::uint8_t value) {
    bytes.push_back(value);
}

void append_u32_le(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (std::size_t index = 0; index < 4U; ++index) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (8U * index)) & 0xFFU));
    }
}

void append_u64_le(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    for (std::size_t index = 0; index < 8U; ++index) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (8U * index)) & 0xFFU));
    }
}

void append_i32_le(std::vector<std::uint8_t>& bytes, const std::int32_t value) {
    append_u32_le(bytes, std::bit_cast<std::uint32_t>(value));
}

bool test_rejects_malformed_array_payload() {
    std::vector<std::uint8_t> payload;
    append_u8(payload, static_cast<std::uint8_t>('C'));
    append_u8(payload, static_cast<std::uint8_t>('C'));
    append_u8(payload, static_cast<std::uint8_t>('M'));
    append_u8(payload, static_cast<std::uint8_t>('R'));
    append_u8(payload, 1U); // format version

    append_u32_le(payload, 1U); // one field
    append_u32_le(payload, 3U); // key length
    append_u8(payload, static_cast<std::uint8_t>('b'));
    append_u8(payload, static_cast<std::uint8_t>('a'));
    append_u8(payload, static_cast<std::uint8_t>('d'));

    append_u8(payload, 22U);    // ValueTag::int32_array
    append_u32_le(payload, 2U); // rank
    append_u64_le(payload, 2U);
    append_u64_le(payload, 2U); // expected count = 4
    append_u64_le(payload, 3U); // malformed count
    append_i32_le(payload, 1);
    append_i32_le(payload, 2);
    append_i32_le(payload, 3);

    try {
        static_cast<void>(casacore_mini::deserialize_record_binary(payload));
    } catch (const std::exception&) {
        return true;
    }
    std::cerr << "deserializer accepted malformed array payload\n";
    return false;
}

} // namespace

int main() noexcept {
    try {
        if (!test_scalar_round_trip()) {
            return 1;
        }
        if (!test_scalar_type_matrix_preserved()) {
            return 1;
        }
        if (!test_array_round_trip()) {
            return 1;
        }
        if (!test_array_type_matrix_preserved()) {
            return 1;
        }
        if (!test_nested_round_trip()) {
            return 1;
        }
        if (!test_deterministic_encoding()) {
            return 1;
        }
        if (!test_rejects_invalid_magic()) {
            return 1;
        }
        if (!test_set_replaces_existing_key()) {
            return 1;
        }
        if (!test_rejects_invalid_array_shape()) {
            return 1;
        }
        if (!test_rejects_malformed_array_payload()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "record_io_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
