#include "casacore_mini/keyword_record.hpp"
#include "casacore_mini/keyword_record_convert.hpp"
#include "casacore_mini/record.hpp"

#include <cassert>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {

// ── Helpers ──────────────────────────────────────────────────────────────

template <typename target_t>
[[nodiscard]] const target_t& get_rv(const casacore_mini::RecordValue& rv) {
    const auto* p = std::get_if<target_t>(&rv.storage());
    assert(p != nullptr);
    return *p;
}

template <typename target_t>
[[nodiscard]] const target_t& get_kv(const casacore_mini::KeywordValue& kv) {
    const auto* p = std::get_if<target_t>(&kv.storage());
    assert(p != nullptr);
    return *p;
}

// ── KeywordRecord → Record ──────────────────────────────────────────────

bool test_scalar_kv_to_rv() {
    using namespace casacore_mini;

    // bool
    {
        auto rv = keyword_value_to_record_value(KeywordValue(true));
        assert(get_rv<bool>(rv) == true);
    }
    // int64
    {
        auto rv = keyword_value_to_record_value(KeywordValue(std::int64_t{42}));
        assert(get_rv<std::int64_t>(rv) == 42);
    }
    // double
    {
        auto rv = keyword_value_to_record_value(KeywordValue(3.14));
        assert(get_rv<double>(rv) == 3.14);
    }
    // string
    {
        auto rv = keyword_value_to_record_value(KeywordValue("hello"));
        assert(get_rv<std::string>(rv) == "hello");
    }
    std::cerr << "  PASS: test_scalar_kv_to_rv\n";
    return true;
}

bool test_array_kv_to_rv() {
    using namespace casacore_mini;

    // int array
    {
        KeywordArray ka;
        ka.elements.push_back(KeywordValue(std::int64_t{1}));
        ka.elements.push_back(KeywordValue(std::int64_t{2}));
        ka.elements.push_back(KeywordValue(std::int64_t{3}));
        auto rv = keyword_value_to_record_value(KeywordValue::from_array(std::move(ka)));
        [[maybe_unused]] const auto& arr = get_rv<RecordValue::int64_array>(rv);
        assert(arr.shape == std::vector<std::uint64_t>{3});
        assert(arr.elements == (std::vector<std::int64_t>{1, 2, 3}));
    }
    // double array
    {
        KeywordArray ka;
        ka.elements.push_back(KeywordValue(1.5));
        ka.elements.push_back(KeywordValue(2.5));
        auto rv = keyword_value_to_record_value(KeywordValue::from_array(std::move(ka)));
        [[maybe_unused]] const auto& arr = get_rv<RecordValue::double_array>(rv);
        assert(arr.shape == std::vector<std::uint64_t>{2});
        assert(arr.elements[0] == 1.5);
        assert(arr.elements[1] == 2.5);
    }
    // string array
    {
        KeywordArray ka;
        ka.elements.push_back(KeywordValue("a"));
        ka.elements.push_back(KeywordValue("b"));
        auto rv = keyword_value_to_record_value(KeywordValue::from_array(std::move(ka)));
        [[maybe_unused]] const auto& arr = get_rv<RecordValue::string_array>(rv);
        assert(arr.shape == std::vector<std::uint64_t>{2});
        assert(arr.elements[0] == "a");
    }
    // empty array → string array
    {
        KeywordArray ka;
        auto rv = keyword_value_to_record_value(KeywordValue::from_array(std::move(ka)));
        [[maybe_unused]] const auto& arr = get_rv<RecordValue::string_array>(rv);
        assert(arr.shape == std::vector<std::uint64_t>{0});
        assert(arr.elements.empty());
    }
    // mixed int + double → double promotion
    {
        KeywordArray ka;
        ka.elements.push_back(KeywordValue(std::int64_t{1}));
        ka.elements.push_back(KeywordValue(2.5));
        auto rv = keyword_value_to_record_value(KeywordValue::from_array(std::move(ka)));
        [[maybe_unused]] const auto& arr = get_rv<RecordValue::double_array>(rv);
        assert(arr.elements[0] == 1.0);
        assert(arr.elements[1] == 2.5);
    }
    std::cerr << "  PASS: test_array_kv_to_rv\n";
    return true;
}

bool test_nested_kv_to_rv() {
    using namespace casacore_mini;

    KeywordRecord child;
    child.set("x", KeywordValue(std::int64_t{99}));

    KeywordRecord parent;
    parent.set("child", KeywordValue::from_record(std::move(child)));
    parent.set("name", KeywordValue("test"));

    auto rec = keyword_record_to_record(parent);
    assert(rec.size() == 2);

    const auto* child_rv = rec.find("child");
    assert(child_rv != nullptr);
    [[maybe_unused]] const auto& child_rec =
        *std::get<RecordValue::record_ptr>(child_rv->storage());
    assert(child_rec.size() == 1);
    assert(get_rv<std::int64_t>(*child_rec.find("x")) == 99);

    assert(get_rv<std::string>(*rec.find("name")) == "test");

    std::cerr << "  PASS: test_nested_kv_to_rv\n";
    return true;
}

// ── Record → KeywordRecord ──────────────────────────────────────────────

bool test_scalar_rv_to_kv() {
    using namespace casacore_mini;

    // bool
    assert(get_kv<bool>(record_value_to_keyword_value(RecordValue(true))) == true);
    // int16 → int64
    assert(get_kv<std::int64_t>(record_value_to_keyword_value(RecordValue(std::int16_t{-5}))) ==
           -5);
    // uint16 → int64
    assert(get_kv<std::int64_t>(record_value_to_keyword_value(RecordValue(std::uint16_t{300}))) ==
           300);
    // int32 → int64
    assert(get_kv<std::int64_t>(record_value_to_keyword_value(RecordValue(std::int32_t{-100}))) ==
           -100);
    // uint32 → int64
    assert(get_kv<std::int64_t>(
               record_value_to_keyword_value(RecordValue(std::uint32_t{200000}))) == 200000);
    // int64 → int64
    assert(get_kv<std::int64_t>(record_value_to_keyword_value(RecordValue(std::int64_t{999}))) ==
           999);
    // uint64 → int64
    assert(get_kv<std::int64_t>(record_value_to_keyword_value(RecordValue(std::uint64_t{42}))) ==
           42);
    // float → double
    assert(get_kv<double>(record_value_to_keyword_value(RecordValue(1.5F))) == 1.5);
    // double → double
    assert(get_kv<double>(record_value_to_keyword_value(RecordValue(3.14))) == 3.14);
    // string
    assert(get_kv<std::string>(record_value_to_keyword_value(RecordValue("hi"))) == "hi");
    // complex → string
    {
        auto kv = record_value_to_keyword_value(RecordValue(std::complex<float>(1.0F, -2.0F)));
        [[maybe_unused]] const auto& str = get_kv<std::string>(kv);
        assert(str.find('1') != std::string::npos);
        assert(str.find("-2") != std::string::npos);
    }
    {
        auto kv = record_value_to_keyword_value(RecordValue(std::complex<double>(3.0, -4.0)));
        [[maybe_unused]] const auto& str = get_kv<std::string>(kv);
        assert(str.find('3') != std::string::npos);
        assert(str.find("-4") != std::string::npos);
    }

    std::cerr << "  PASS: test_scalar_rv_to_kv\n";
    return true;
}

bool test_array_rv_to_kv() {
    using namespace casacore_mini;

    // int32 array → flat KeywordArray of int64
    {
        RecordValue::int32_array arr;
        arr.shape = {2, 3};
        arr.elements = {1, 2, 3, 4, 5, 6};
        auto kv = record_value_to_keyword_value(RecordValue(std::move(arr)));
        [[maybe_unused]] const auto& ka = *std::get<KeywordValue::array_ptr>(kv.storage());
        assert(ka.elements.size() == 6);
        assert(get_kv<std::int64_t>(ka.elements[0]) == 1);
        assert(get_kv<std::int64_t>(ka.elements[5]) == 6);
    }
    // double array
    {
        RecordValue::double_array arr;
        arr.shape = {2};
        arr.elements = {1.5, 2.5};
        auto kv = record_value_to_keyword_value(RecordValue(std::move(arr)));
        [[maybe_unused]] const auto& ka = *std::get<KeywordValue::array_ptr>(kv.storage());
        assert(ka.elements.size() == 2);
        assert(get_kv<double>(ka.elements[0]) == 1.5);
    }
    // string array
    {
        RecordValue::string_array arr;
        arr.shape = {2};
        arr.elements = {"a", "b"};
        auto kv = record_value_to_keyword_value(RecordValue(std::move(arr)));
        [[maybe_unused]] const auto& ka = *std::get<KeywordValue::array_ptr>(kv.storage());
        assert(ka.elements.size() == 2);
        assert(get_kv<std::string>(ka.elements[0]) == "a");
    }
    // complex64 array → string array
    {
        RecordValue::complex64_array arr;
        arr.shape = {1};
        arr.elements = {std::complex<float>(1.0F, 2.0F)};
        auto kv = record_value_to_keyword_value(RecordValue(std::move(arr)));
        [[maybe_unused]] const auto& ka = *std::get<KeywordValue::array_ptr>(kv.storage());
        assert(ka.elements.size() == 1);
        [[maybe_unused]] const auto& str = get_kv<std::string>(ka.elements[0]);
        assert(str.find('1') != std::string::npos);
    }

    std::cerr << "  PASS: test_array_rv_to_kv\n";
    return true;
}

bool test_nested_rv_to_kv() {
    using namespace casacore_mini;

    Record child;
    child.set("pi", RecordValue(3.14));
    child.set("n", RecordValue(std::int32_t{7}));

    Record parent;
    parent.set("child", RecordValue::from_record(std::move(child)));
    parent.set("label", RecordValue("test"));

    auto kr = record_to_keyword_record(parent);
    assert(kr.size() == 2);

    const auto* child_kv = kr.find("child");
    assert(child_kv != nullptr);
    [[maybe_unused]] const auto& child_kr =
        *std::get<KeywordValue::record_ptr>(child_kv->storage());
    assert(child_kr.size() == 2);
    assert(get_kv<double>(*child_kr.find("pi")) == 3.14);
    assert(get_kv<std::int64_t>(*child_kr.find("n")) == 7);
    assert(get_kv<std::string>(*kr.find("label")) == "test");

    std::cerr << "  PASS: test_nested_rv_to_kv\n";
    return true;
}

// ── Roundtrip tests ─────────────────────────────────────────────────────

bool test_kv_to_rv_roundtrip() {
    using namespace casacore_mini;

    // Build a KeywordRecord → convert to Record → convert back.
    // Because of type widening, the roundtrip preserves types within the
    // KeywordRecord type system (bool, int64, double, string).
    KeywordRecord original;
    original.set("flag", KeywordValue(true));
    original.set("count", KeywordValue(std::int64_t{42}));
    original.set("pi", KeywordValue(3.14));
    original.set("name", KeywordValue("hello"));

    KeywordArray ka;
    ka.elements.push_back(KeywordValue(std::int64_t{1}));
    ka.elements.push_back(KeywordValue(std::int64_t{2}));
    original.set("ids", KeywordValue::from_array(std::move(ka)));

    auto rec = keyword_record_to_record(original);
    auto roundtripped = record_to_keyword_record(rec);

    // Scalar fields should match exactly.
    assert(get_kv<bool>(*roundtripped.find("flag")) == true);
    assert(get_kv<std::int64_t>(*roundtripped.find("count")) == 42);
    assert(get_kv<double>(*roundtripped.find("pi")) == 3.14);
    assert(get_kv<std::string>(*roundtripped.find("name")) == "hello");

    // Array: roundtripped through int64_array → KeywordArray of int64
    const auto* ids_kv = roundtripped.find("ids");
    assert(ids_kv != nullptr);
    [[maybe_unused]] const auto& ids = *std::get<KeywordValue::array_ptr>(ids_kv->storage());
    assert(ids.elements.size() == 2);
    assert(get_kv<std::int64_t>(ids.elements[0]) == 1);
    assert(get_kv<std::int64_t>(ids.elements[1]) == 2);

    std::cerr << "  PASS: test_kv_to_rv_roundtrip\n";
    return true;
}

bool test_rv_to_kv_roundtrip() {
    using namespace casacore_mini;

    // Build a Record with types that survive KV→RV roundtrip (int64, double,
    // bool, string). Narrow types are widened on the KV side.
    Record original;
    original.set("flag", RecordValue(true));
    original.set("n", RecordValue(std::int64_t{7}));
    original.set("x", RecordValue(3.14));
    original.set("s", RecordValue("world"));

    auto kr = record_to_keyword_record(original);
    auto roundtripped = keyword_record_to_record(kr);

    assert(get_rv<bool>(*roundtripped.find("flag")) == true);
    assert(get_rv<std::int64_t>(*roundtripped.find("n")) == 7);
    assert(get_rv<double>(*roundtripped.find("x")) == 3.14);
    assert(get_rv<std::string>(*roundtripped.find("s")) == "world");

    std::cerr << "  PASS: test_rv_to_kv_roundtrip\n";
    return true;
}

bool test_list_to_kv() {
    using namespace casacore_mini;

    // RecordList → KeywordArray
    RecordList rl;
    rl.elements.push_back(RecordValue(std::int32_t{1}));
    rl.elements.push_back(RecordValue("two"));
    auto rv = RecordValue::from_list(std::move(rl));
    auto kv = record_value_to_keyword_value(rv);
    [[maybe_unused]] const auto& ka = *std::get<KeywordValue::array_ptr>(kv.storage());
    assert(ka.elements.size() == 2);
    assert(get_kv<std::int64_t>(ka.elements[0]) == 1);
    assert(get_kv<std::string>(ka.elements[1]) == "two");

    std::cerr << "  PASS: test_list_to_kv\n";
    return true;
}

} // namespace

int main() noexcept {
    try {
        std::cerr << "keyword_record_convert_test:\n";
        if (!test_scalar_kv_to_rv()) {
            return EXIT_FAILURE;
        }
        if (!test_array_kv_to_rv()) {
            return EXIT_FAILURE;
        }
        if (!test_nested_kv_to_rv()) {
            return EXIT_FAILURE;
        }
        if (!test_scalar_rv_to_kv()) {
            return EXIT_FAILURE;
        }
        if (!test_array_rv_to_kv()) {
            return EXIT_FAILURE;
        }
        if (!test_nested_rv_to_kv()) {
            return EXIT_FAILURE;
        }
        if (!test_kv_to_rv_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_rv_to_kv_roundtrip()) {
            return EXIT_FAILURE;
        }
        if (!test_list_to_kv()) {
            return EXIT_FAILURE;
        }
    } catch (const std::exception& error) {
        std::cerr << "keyword_record_convert_test threw: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
