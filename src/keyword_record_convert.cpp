// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/keyword_record_convert.hpp"

#include <complex>
#include <cstdint>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace casacore_mini {

// ── KeywordValue → RecordValue ──────────────────────────────────────────

namespace {

/// Determine the dominant element type in a KeywordArray for Record conversion.
/// Returns: 0=bool, 1=int, 2=double, 3=string, 4=record
[[nodiscard]] int classify_keyword_array(const KeywordArray& arr) {
    if (arr.elements.empty()) {
        return 3; // empty → string
    }
    int best = -1;
    for (const auto& elem : arr.elements) {
        const auto& s = elem.storage();
        int kind = 3; // default string
        if (std::holds_alternative<bool>(s)) {
            kind = 0;
        } else if (std::holds_alternative<std::int64_t>(s)) {
            kind = 1;
        } else if (std::holds_alternative<double>(s)) {
            kind = 2;
        } else if (std::holds_alternative<std::string>(s)) {
            kind = 3;
        } else if (std::holds_alternative<KeywordValue::record_ptr>(s)) {
            kind = 4;
        }
        if (kind > best) {
            best = kind;
        }
    }
    return best;
}

/// Convert a single KeywordValue element to int64_t.
[[nodiscard]] std::int64_t kv_to_int64(const KeywordValue& kv) {
    const auto& s = kv.storage();
    if (const auto* v = std::get_if<bool>(&s)) {
        return *v ? 1 : 0;
    }
    if (const auto* v = std::get_if<std::int64_t>(&s)) {
        return *v;
    }
    if (const auto* v = std::get_if<double>(&s)) {
        return static_cast<std::int64_t>(*v);
    }
    return 0;
}

/// Convert a single KeywordValue element to double.
[[nodiscard]] double kv_to_double(const KeywordValue& kv) {
    const auto& s = kv.storage();
    if (const auto* v = std::get_if<bool>(&s)) {
        return *v ? 1.0 : 0.0;
    }
    if (const auto* v = std::get_if<std::int64_t>(&s)) {
        return static_cast<double>(*v);
    }
    if (const auto* v = std::get_if<double>(&s)) {
        return *v;
    }
    return 0.0;
}

/// Convert a single KeywordValue element to string.
[[nodiscard]] std::string kv_to_string(const KeywordValue& kv) {
    const auto& s = kv.storage();
    if (const auto* v = std::get_if<bool>(&s)) {
        return *v ? "true" : "false";
    }
    if (const auto* v = std::get_if<std::int64_t>(&s)) {
        return std::to_string(*v);
    }
    if (const auto* v = std::get_if<double>(&s)) {
        std::ostringstream oss;
        oss << *v;
        return oss.str();
    }
    if (const auto* v = std::get_if<std::string>(&s)) {
        return *v;
    }
    return {};
}

} // namespace

RecordValue keyword_value_to_record_value(const KeywordValue& kv) {
    const auto& s = kv.storage();

    if (const auto* v = std::get_if<bool>(&s)) {
        return RecordValue(*v);
    }
    if (const auto* v = std::get_if<std::int64_t>(&s)) {
        return RecordValue(*v);
    }
    if (const auto* v = std::get_if<double>(&s)) {
        return RecordValue(*v);
    }
    if (const auto* v = std::get_if<std::string>(&s)) {
        return RecordValue(*v);
    }
    if (const auto* v = std::get_if<KeywordValue::array_ptr>(&s)) {
        if (*v == nullptr) {
            RecordValue::string_array empty;
            return RecordValue(std::move(empty));
        }
        const auto& arr = **v;
        const auto kind = classify_keyword_array(arr);
        const auto n = arr.elements.size();
        const auto shape_vec = std::vector<std::uint64_t>{static_cast<std::uint64_t>(n)};

        if (kind <= 1) { // bool or int → int64 array
            RecordValue::int64_array result;
            result.shape = shape_vec;
            result.elements.reserve(n);
            for (const auto& e : arr.elements) {
                result.elements.push_back(kv_to_int64(e));
            }
            return RecordValue(std::move(result));
        }
        if (kind == 2) { // double array
            RecordValue::double_array result;
            result.shape = shape_vec;
            result.elements.reserve(n);
            for (const auto& e : arr.elements) {
                result.elements.push_back(kv_to_double(e));
            }
            return RecordValue(std::move(result));
        }
        // kind 3 or 4 → string array
        RecordValue::string_array result;
        result.shape = shape_vec;
        result.elements.reserve(n);
        for (const auto& e : arr.elements) {
            result.elements.push_back(kv_to_string(e));
        }
        return RecordValue(std::move(result));
    }
    if (const auto* v = std::get_if<KeywordValue::record_ptr>(&s)) {
        if (*v == nullptr) {
            return RecordValue::from_record(Record{});
        }
        return RecordValue::from_record(keyword_record_to_record(**v));
    }

    return RecordValue(false);
}

Record keyword_record_to_record(const KeywordRecord& kr) {
    Record result;
    for (const auto& [key, kv] : kr.entries()) {
        result.set(key, keyword_value_to_record_value(kv));
    }
    return result;
}

// ── RecordValue → KeywordValue ──────────────────────────────────────────

namespace {

/// Format a complex number as "(real,imag)" string.
template <typename float_t>
[[nodiscard]] std::string format_complex_string(const std::complex<float_t>& c) {
    std::ostringstream oss;
    oss << '(' << c.real() << ',' << c.imag() << ')';
    return oss.str();
}

} // namespace

KeywordValue record_value_to_keyword_value(const RecordValue& rv) {
    const auto& s = rv.storage();

    if (const auto* v = std::get_if<bool>(&s)) {
        return KeywordValue(*v);
    }
    if (const auto* v = std::get_if<std::int16_t>(&s)) {
        return KeywordValue(static_cast<std::int64_t>(*v));
    }
    if (const auto* v = std::get_if<std::uint16_t>(&s)) {
        return KeywordValue(static_cast<std::int64_t>(*v));
    }
    if (const auto* v = std::get_if<std::int32_t>(&s)) {
        return KeywordValue(static_cast<std::int64_t>(*v));
    }
    if (const auto* v = std::get_if<std::uint32_t>(&s)) {
        return KeywordValue(static_cast<std::int64_t>(*v));
    }
    if (const auto* v = std::get_if<std::int64_t>(&s)) {
        return KeywordValue(*v);
    }
    if (const auto* v = std::get_if<std::uint64_t>(&s)) {
        return KeywordValue(static_cast<std::int64_t>(*v));
    }
    if (const auto* v = std::get_if<float>(&s)) {
        return KeywordValue(static_cast<double>(*v));
    }
    if (const auto* v = std::get_if<double>(&s)) {
        return KeywordValue(*v);
    }
    if (const auto* v = std::get_if<std::string>(&s)) {
        return KeywordValue(*v);
    }
    if (const auto* v = std::get_if<std::complex<float>>(&s)) {
        return KeywordValue(format_complex_string(*v));
    }
    if (const auto* v = std::get_if<std::complex<double>>(&s)) {
        return KeywordValue(format_complex_string(*v));
    }

    // Array types → flat KeywordArray
    auto make_kw_array = [](auto& typed_arr, auto converter) -> KeywordValue {
        KeywordArray ka;
        ka.elements.reserve(typed_arr.elements.size());
        for (const auto& e : typed_arr.elements) {
            ka.elements.push_back(converter(e));
        }
        return KeywordValue::from_array(std::move(ka));
    };

    if (const auto* v = std::get_if<RecordValue::int16_array>(&s)) {
        return make_kw_array(
            *v, [](std::int16_t e) { return KeywordValue(static_cast<std::int64_t>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::uint16_array>(&s)) {
        return make_kw_array(
            *v, [](std::uint16_t e) { return KeywordValue(static_cast<std::int64_t>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::int32_array>(&s)) {
        return make_kw_array(
            *v, [](std::int32_t e) { return KeywordValue(static_cast<std::int64_t>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::uint32_array>(&s)) {
        return make_kw_array(
            *v, [](std::uint32_t e) { return KeywordValue(static_cast<std::int64_t>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::int64_array>(&s)) {
        return make_kw_array(*v, [](std::int64_t e) { return KeywordValue(e); });
    }
    if (const auto* v = std::get_if<RecordValue::uint64_array>(&s)) {
        return make_kw_array(
            *v, [](std::uint64_t e) { return KeywordValue(static_cast<std::int64_t>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::float_array>(&s)) {
        return make_kw_array(*v, [](float e) { return KeywordValue(static_cast<double>(e)); });
    }
    if (const auto* v = std::get_if<RecordValue::double_array>(&s)) {
        return make_kw_array(*v, [](double e) { return KeywordValue(e); });
    }
    if (const auto* v = std::get_if<RecordValue::string_array>(&s)) {
        return make_kw_array(*v, [](const std::string& e) { return KeywordValue(e); });
    }
    if (const auto* v = std::get_if<RecordValue::complex64_array>(&s)) {
        return make_kw_array(*v, [](const std::complex<float>& e) {
            return KeywordValue(format_complex_string(e));
        });
    }
    if (const auto* v = std::get_if<RecordValue::complex128_array>(&s)) {
        return make_kw_array(*v, [](const std::complex<double>& e) {
            return KeywordValue(format_complex_string(e));
        });
    }

    // RecordList → flat KeywordArray
    if (const auto* v = std::get_if<RecordValue::list_ptr>(&s)) {
        if (*v == nullptr) {
            return KeywordValue::from_array(KeywordArray{});
        }
        KeywordArray ka;
        ka.elements.reserve((*v)->elements.size());
        for (const auto& elem : (*v)->elements) {
            ka.elements.push_back(record_value_to_keyword_value(elem));
        }
        return KeywordValue::from_array(std::move(ka));
    }

    // Nested Record → KeywordRecord
    if (const auto* v = std::get_if<RecordValue::record_ptr>(&s)) {
        if (*v == nullptr) {
            return KeywordValue::from_record(KeywordRecord{});
        }
        return KeywordValue::from_record(record_to_keyword_record(**v));
    }

    return KeywordValue(false);
}

KeywordRecord record_to_keyword_record(const Record& rec) {
    KeywordRecord result;
    for (const auto& [key, rv] : rec.entries()) {
        result.set(key, record_value_to_keyword_value(rv));
    }
    return result;
}

} // namespace casacore_mini
