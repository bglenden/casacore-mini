#include "casacore_mini/record.hpp"
#include "casacore_mini/record_io.hpp"

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

casacore_mini::Record build_scalar_record() {
    casacore_mini::Record value;
    value.set("flag", casacore_mini::RecordValue(true));
    value.set("answer", casacore_mini::RecordValue(std::int64_t(42)));
    value.set("ratio", casacore_mini::RecordValue(1.5));
    value.set("label", casacore_mini::RecordValue("mini"));
    value.set("phasorf", casacore_mini::RecordValue(std::complex<float>(1.0F, -2.0F)));
    value.set("phasor", casacore_mini::RecordValue(std::complex<double>(1.0, -2.0)));
    return value;
}

casacore_mini::Record build_nested_record() {
    casacore_mini::Record child;
    child.set("pi", casacore_mini::RecordValue(3.141592653589793));

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

bool test_nested_round_trip() {
    const auto expected = build_nested_record();
    const auto payload = casacore_mini::serialize_record_binary(expected);
    const auto actual = casacore_mini::deserialize_record_binary(payload);
    return expect_true(actual == expected, "nested Record round-trip failed");
}

bool test_complex_precision_preserved() {
    casacore_mini::Record value;
    value.set("phasorf", casacore_mini::RecordValue(std::complex<float>(3.0F, 4.0F)));
    value.set("phasor", casacore_mini::RecordValue(std::complex<double>(3.0, 4.0)));

    const auto payload = casacore_mini::serialize_record_binary(value);
    const auto round_trip = casacore_mini::deserialize_record_binary(payload);

    const auto* phasorf = round_trip.find("phasorf");
    if (!expect_true(phasorf != nullptr, "complex<float> field missing after round-trip")) {
        return false;
    }

    if (!expect_true(std::holds_alternative<std::complex<float>>(phasorf->storage()),
                     "complex<float> field changed type after round-trip")) {
        return false;
    }

    const auto* phasor = round_trip.find("phasor");
    if (!expect_true(phasor != nullptr, "complex<double> field missing after round-trip")) {
        return false;
    }

    return expect_true(std::holds_alternative<std::complex<double>>(phasor->storage()),
                       "complex<double> field changed type after round-trip");
}

bool test_deterministic_encoding() {
    const auto value = build_nested_record();
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

} // namespace

int main() noexcept {
    try {
        if (!test_scalar_round_trip()) {
            return 1;
        }
        if (!test_nested_round_trip()) {
            return 1;
        }
        if (!test_deterministic_encoding()) {
            return 1;
        }
        if (!test_complex_precision_preserved()) {
            return 1;
        }
        if (!test_rejects_invalid_magic()) {
            return 1;
        }
        if (!test_set_replaces_existing_key()) {
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "record_io_test threw exception: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
