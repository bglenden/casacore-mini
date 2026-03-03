// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measure_record.hpp"
#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/record.hpp"
#include "casacore_mini/table_measure_desc.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace casacore_mini;

// ---------------------------------------------------------------------------
// TableMeasDesc round-trip: fixed epoch reference
// ---------------------------------------------------------------------------
bool test_epoch_fixed_ref_roundtrip() {
    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.units = {"s"};

    Record kw;
    write_table_measure_desc(desc, kw);

    auto parsed = read_table_measure_desc("TIME", kw);
    assert(parsed.has_value());
    assert(parsed->column_name == "TIME");
    assert(parsed->measure_type == MeasureType::epoch);
    assert(std::holds_alternative<EpochRef>(parsed->default_ref));
    assert(std::get<EpochRef>(parsed->default_ref) == EpochRef::utc);
    assert(!parsed->has_variable_ref());
    assert(parsed->units.size() == 1);
    assert(parsed->units[0] == "s");
    assert(!parsed->has_offset());
    return true;
}

// ---------------------------------------------------------------------------
// Direction with fixed J2000 reference
// ---------------------------------------------------------------------------
bool test_direction_fixed_ref_roundtrip() {
    TableMeasDesc desc;
    desc.column_name = "PHASE_DIR";
    desc.measure_type = MeasureType::direction;
    desc.default_ref = DirectionRef::j2000;
    desc.units = {"rad", "rad"};

    Record kw;
    write_table_measure_desc(desc, kw);

    auto parsed = read_table_measure_desc("PHASE_DIR", kw);
    assert(parsed.has_value());
    assert(parsed->measure_type == MeasureType::direction);
    assert(std::get<DirectionRef>(parsed->default_ref) == DirectionRef::j2000);
    assert(parsed->units.size() == 2);
    assert(parsed->units[0] == "rad");
    assert(parsed->units[1] == "rad");
    return true;
}

// ---------------------------------------------------------------------------
// Variable reference column
// ---------------------------------------------------------------------------
bool test_variable_ref_roundtrip() {
    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.var_ref_column = "TIME_REF";
    desc.tab_ref_types = {"UTC", "TAI", "TDT"};
    desc.tab_ref_codes = {0, 1, 2};
    desc.units = {"s"};

    Record kw;
    write_table_measure_desc(desc, kw);

    auto parsed = read_table_measure_desc("TIME", kw);
    assert(parsed.has_value());
    assert(parsed->has_variable_ref());
    assert(parsed->var_ref_column == "TIME_REF");
    assert(parsed->tab_ref_types.size() == 3);
    assert(parsed->tab_ref_types[0] == "UTC");
    assert(parsed->tab_ref_types[1] == "TAI");
    assert(parsed->tab_ref_types[2] == "TDT");
    assert(parsed->tab_ref_codes.size() == 3);
    assert(parsed->tab_ref_codes[0] == 0);
    assert(parsed->tab_ref_codes[1] == 1);
    assert(parsed->tab_ref_codes[2] == 2);
    return true;
}

// ---------------------------------------------------------------------------
// Fixed offset measure
// ---------------------------------------------------------------------------
bool test_fixed_offset_roundtrip() {
    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.units = {"d"};
    desc.offset = Measure{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
                          EpochValue{50000.0, 0.0}};

    Record kw;
    write_table_measure_desc(desc, kw);

    auto parsed = read_table_measure_desc("TIME", kw);
    assert(parsed.has_value());
    assert(parsed->has_offset());
    assert(parsed->offset.has_value());
    assert(parsed->offset->type == MeasureType::epoch);
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(parsed->offset->value);
    assert(ev.day == 50000.0);
    return true;
}

// ---------------------------------------------------------------------------
// Variable offset column
// ---------------------------------------------------------------------------
bool test_variable_offset_roundtrip() {
    TableMeasDesc desc;
    desc.column_name = "UVW";
    desc.measure_type = MeasureType::uvw;
    desc.default_ref = UvwRef::j2000;
    desc.units = {"m", "m", "m"};
    desc.offset_column = "UVW_OFFSET";
    desc.offset_per_array = true;

    Record kw;
    write_table_measure_desc(desc, kw);

    auto parsed = read_table_measure_desc("UVW", kw);
    assert(parsed.has_value());
    assert(parsed->has_offset());
    assert(!parsed->offset.has_value()); // No fixed offset
    assert(parsed->offset_column == "UVW_OFFSET");
    assert(parsed->offset_per_array);
    return true;
}

// ---------------------------------------------------------------------------
// All 9 measure types: basic round-trip
// ---------------------------------------------------------------------------
bool test_all_measure_types_roundtrip() {
    struct TestCase {
        MeasureType type;
        MeasureRefType ref;
        std::vector<std::string> units;
    };

    TestCase cases[] = {
        {MeasureType::epoch, EpochRef::utc, {"s"}},
        {MeasureType::direction, DirectionRef::j2000, {"rad", "rad"}},
        {MeasureType::position, PositionRef::itrf, {"m", "m", "m"}},
        {MeasureType::frequency, FrequencyRef::lsrk, {"Hz"}},
        {MeasureType::doppler, DopplerRef::radio, {}},
        {MeasureType::radial_velocity, RadialVelocityRef::lsrk, {"m/s"}},
        {MeasureType::baseline, BaselineRef::j2000, {"m", "m", "m"}},
        {MeasureType::uvw, UvwRef::j2000, {"m", "m", "m"}},
        {MeasureType::earth_magnetic, EarthMagneticRef::j2000, {"T", "T", "T"}},
    };

    for (const auto& tc : cases) {
        TableMeasDesc desc;
        desc.column_name = "COL";
        desc.measure_type = tc.type;
        desc.default_ref = tc.ref;
        desc.units = tc.units;

        Record kw;
        write_table_measure_desc(desc, kw);

        auto parsed = read_table_measure_desc("COL", kw);
        assert(parsed.has_value());
        assert(parsed->measure_type == tc.type);
        assert(parsed->units.size() == tc.units.size());
    }
    return true;
}

// ---------------------------------------------------------------------------
// No MEASINFO => nullopt
// ---------------------------------------------------------------------------
bool test_no_measinfo_returns_nullopt() {
    Record kw;
    kw.set("SomeOtherKey", RecordValue(std::string("value")));

    auto parsed = read_table_measure_desc("TIME", kw);
    assert(!parsed.has_value());
    return true;
}

// ---------------------------------------------------------------------------
// Missing type field throws
// ---------------------------------------------------------------------------
bool test_missing_type_throws() {
    Record measinfo;
    measinfo.set("Ref", RecordValue(std::string("UTC")));

    Record kw;
    kw.set("MEASINFO", RecordValue::from_record(std::move(measinfo)));

    try {
        (void)read_table_measure_desc("TIME", kw);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

// ---------------------------------------------------------------------------
// TableQuantumDesc round-trip
// ---------------------------------------------------------------------------
bool test_quantum_desc_fixed_roundtrip() {
    TableQuantumDesc desc;
    desc.column_name = "DATA";
    desc.units = {"Jy"};

    Record kw;
    write_table_quantum_desc(desc, kw);

    auto parsed = read_table_quantum_desc("DATA", kw);
    assert(parsed.has_value());
    assert(parsed->units.size() == 1);
    assert(parsed->units[0] == "Jy");
    assert(!parsed->has_variable_units());
    return true;
}

bool test_quantum_desc_variable_roundtrip() {
    TableQuantumDesc desc;
    desc.column_name = "DATA";
    desc.var_units_column = "UNIT_COL";

    Record kw;
    write_table_quantum_desc(desc, kw);

    auto parsed = read_table_quantum_desc("DATA", kw);
    assert(parsed.has_value());
    assert(parsed->has_variable_units());
    assert(parsed->var_units_column == "UNIT_COL");
    return true;
}

bool test_quantum_desc_none() {
    Record kw;
    auto parsed = read_table_quantum_desc("DATA", kw);
    assert(!parsed.has_value());
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

    std::cout << "table_measure_desc_test\n";
    run("epoch_fixed_ref_roundtrip", test_epoch_fixed_ref_roundtrip);
    run("direction_fixed_ref_roundtrip", test_direction_fixed_ref_roundtrip);
    run("variable_ref_roundtrip", test_variable_ref_roundtrip);
    run("fixed_offset_roundtrip", test_fixed_offset_roundtrip);
    run("variable_offset_roundtrip", test_variable_offset_roundtrip);
    run("all_measure_types_roundtrip", test_all_measure_types_roundtrip);
    run("no_measinfo_returns_nullopt", test_no_measinfo_returns_nullopt);
    run("missing_type_throws", test_missing_type_throws);
    run("quantum_desc_fixed_roundtrip", test_quantum_desc_fixed_roundtrip);
    run("quantum_desc_variable_roundtrip", test_quantum_desc_variable_roundtrip);
    run("quantum_desc_none", test_quantum_desc_none);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
