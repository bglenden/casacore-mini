#include "casacore_mini/measure_record.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using namespace casacore_mini;

bool test_missing_type() {
    Record rec;
    rec.set("refer", RecordValue(std::string("UTC")));
    Record m0;
    m0.set("value", RecordValue(double{59000.5}));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false; // Should have thrown
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_empty_type() {
    Record rec;
    rec.set("type", RecordValue(std::string("")));
    rec.set("refer", RecordValue(std::string("UTC")));
    Record m0;
    m0.set("value", RecordValue(double{59000.5}));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_unknown_type() {
    Record rec;
    rec.set("type", RecordValue(std::string("nonexistent_measure")));
    rec.set("refer", RecordValue(std::string("UTC")));
    Record m0;
    m0.set("value", RecordValue(double{0.0}));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_missing_refer() {
    Record rec;
    rec.set("type", RecordValue(std::string("epoch")));
    Record m0;
    m0.set("value", RecordValue(double{59000.5}));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_unknown_refer() {
    Record rec;
    rec.set("type", RecordValue(std::string("epoch")));
    rec.set("refer", RecordValue(std::string("BOGUS_REF")));
    Record m0;
    m0.set("value", RecordValue(double{59000.5}));
    m0.set("unit", RecordValue(std::string("d")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_missing_m0() {
    Record rec;
    rec.set("type", RecordValue(std::string("epoch")));
    rec.set("refer", RecordValue(std::string("UTC")));
    // No m0 field

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_direction_missing_m1() {
    // Direction needs 2 components but only provide m0
    Record rec;
    rec.set("type", RecordValue(std::string("direction")));
    rec.set("refer", RecordValue(std::string("J2000")));
    Record m0;
    m0.set("value", RecordValue(double{0.0}));
    m0.set("unit", RecordValue(std::string("rad")));
    rec.set("m0", RecordValue::from_record(std::move(m0)));

    try {
        (void)measure_from_record(rec);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

bool test_roundtrip_with_nan() {
    // NaN values should survive serialization round-trip
    Measure m;
    m.type = MeasureType::epoch;
    m.ref.ref_type = EpochRef::utc;
    m.value = EpochValue{std::numeric_limits<double>::quiet_NaN(), 0.0};

    auto rec = measure_to_record(m);
    auto back = measure_from_record(rec);

    auto ev_nan = std::get<EpochValue>(back.value);
    if (!std::isnan(ev_nan.day)) return false;
    return true;
}

bool test_roundtrip_with_inf() {
    Measure m;
    m.type = MeasureType::epoch;
    m.ref.ref_type = EpochRef::utc;
    m.value = EpochValue{std::numeric_limits<double>::infinity(), 0.0};

    auto rec = measure_to_record(m);
    auto back = measure_from_record(rec);

    auto ev_inf = std::get<EpochValue>(back.value);
    if (!std::isinf(ev_inf.day)) return false;
    return true;
}

} // namespace

int main() {
    int failures = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "  " << name << " ... ";
        try {
            if (fn()) {
                std::cout << "PASS\n";
            } else {
                std::cout << "FAIL\n";
                ++failures;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << "\n";
            ++failures;
        }
    };

    std::cout << "measure_malformed_test\n";
    run("missing_type", test_missing_type);
    run("empty_type", test_empty_type);
    run("unknown_type", test_unknown_type);
    run("missing_refer", test_missing_refer);
    run("unknown_refer", test_unknown_refer);
    run("missing_m0", test_missing_m0);
    run("direction_missing_m1", test_direction_missing_m1);
    run("roundtrip_with_nan", test_roundtrip_with_nan);
    run("roundtrip_with_inf", test_roundtrip_with_inf);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
