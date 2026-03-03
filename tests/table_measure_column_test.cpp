// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/table_measure_column.hpp"
#include "casacore_mini/table_measure_desc.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-12;

[[maybe_unused]] bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// Mock cell storage for testing
// ---------------------------------------------------------------------------
struct MockTable {
    // Scalar storage: column_name -> row -> CellValue
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, CellValue>> scalars;
    // Array storage: column_name -> row -> flat doubles
    std::unordered_map<std::string, std::unordered_map<std::uint64_t, std::vector<double>>> arrays;

    ScalarCellReader make_scalar_reader() const {
        return [this](const std::string& col, std::uint64_t row) -> CellValue {
            auto it = scalars.find(col);
            if (it == scalars.end()) {
                throw std::invalid_argument("Column not found: " + col);
            }
            auto rit = it->second.find(row);
            if (rit == it->second.end()) {
                throw std::invalid_argument("Row not found");
            }
            return rit->second;
        };
    }

    ArrayCellReader make_array_reader() const {
        return [this](const std::string& col, std::uint64_t row) -> std::vector<double> {
            auto it = arrays.find(col);
            if (it == arrays.end()) {
                throw std::invalid_argument("Array column not found: " + col);
            }
            auto rit = it->second.find(row);
            if (rit == it->second.end()) {
                throw std::invalid_argument("Row not found");
            }
            return rit->second;
        };
    }
};

// ---------------------------------------------------------------------------
// Read scalar epoch measure (single-component: MJD as double)
// ---------------------------------------------------------------------------
bool test_read_scalar_epoch() {
    MockTable table;
    table.scalars["TIME"][0] = CellValue{59000.5};

    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.units = {"d"};

    Measure m = read_scalar_measure(desc, table.make_scalar_reader(), 0);
    assert(m.type == MeasureType::epoch);
    assert(std::holds_alternative<EpochRef>(m.ref.ref_type));
    assert(std::get<EpochRef>(m.ref.ref_type) == EpochRef::utc);
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(m.value);
    assert(near(ev.day, 59000.5));
    return true;
}

// ---------------------------------------------------------------------------
// Read scalar frequency measure
// ---------------------------------------------------------------------------
bool test_read_scalar_frequency() {
    MockTable table;
    table.scalars["FREQ"][0] = CellValue{1.42e9};

    TableMeasDesc desc;
    desc.column_name = "FREQ";
    desc.measure_type = MeasureType::frequency;
    desc.default_ref = FrequencyRef::lsrk;
    desc.units = {"Hz"};

    Measure m = read_scalar_measure(desc, table.make_scalar_reader(), 0);
    assert(m.type == MeasureType::frequency);
    assert(std::get<FrequencyRef>(m.ref.ref_type) == FrequencyRef::lsrk);
    [[maybe_unused]] const auto& fv = std::get<FrequencyValue>(m.value);
    assert(near(fv.hz, 1.42e9));
    return true;
}

// ---------------------------------------------------------------------------
// Read array UVW measures
// ---------------------------------------------------------------------------
bool test_read_array_uvw() {
    MockTable table;
    // 2 UVW baselines: [u1,v1,w1, u2,v2,w2]
    table.arrays["UVW"][0] = {100.0, 200.0, 300.0, 400.0, 500.0, 600.0};

    TableMeasDesc desc;
    desc.column_name = "UVW";
    desc.measure_type = MeasureType::uvw;
    desc.default_ref = UvwRef::j2000;
    desc.units = {"m", "m", "m"};

    auto measures =
        read_array_measures(desc, table.make_array_reader(), table.make_scalar_reader(), 0);
    assert(measures.size() == 2);

    [[maybe_unused]] const auto& uvw0 = std::get<UvwValue>(measures[0].value);
    assert(near(uvw0.u_m, 100.0));
    assert(near(uvw0.v_m, 200.0));
    assert(near(uvw0.w_m, 300.0));

    [[maybe_unused]] const auto& uvw1 = std::get<UvwValue>(measures[1].value);
    assert(near(uvw1.u_m, 400.0));
    assert(near(uvw1.v_m, 500.0));
    assert(near(uvw1.w_m, 600.0));

    return true;
}

// ---------------------------------------------------------------------------
// Read array direction measures
// ---------------------------------------------------------------------------
bool test_read_array_direction() {
    MockTable table;
    // 3 directions: [lon1,lat1, lon2,lat2, lon3,lat3]
    table.arrays["PHASE_DIR"][0] = {1.0, 0.5, 2.0, -0.3, 3.0, 0.1};

    TableMeasDesc desc;
    desc.column_name = "PHASE_DIR";
    desc.measure_type = MeasureType::direction;
    desc.default_ref = DirectionRef::j2000;
    desc.units = {"rad", "rad"};

    auto measures =
        read_array_measures(desc, table.make_array_reader(), table.make_scalar_reader(), 0);
    assert(measures.size() == 3);

    [[maybe_unused]] const auto& d0 = std::get<DirectionValue>(measures[0].value);
    assert(near(d0.lon_rad, 1.0));
    assert(near(d0.lat_rad, 0.5));

    [[maybe_unused]] const auto& d2 = std::get<DirectionValue>(measures[2].value);
    assert(near(d2.lon_rad, 3.0));
    assert(near(d2.lat_rad, 0.1));

    return true;
}

// ---------------------------------------------------------------------------
// Variable reference: string column
// ---------------------------------------------------------------------------
bool test_variable_ref_string() {
    MockTable table;
    table.scalars["TIME"][0] = CellValue{59000.5};
    table.scalars["TIME_REF"][0] = CellValue{std::string("TAI")};

    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.var_ref_column = "TIME_REF";
    desc.units = {"d"};

    Measure m = read_scalar_measure(desc, table.make_scalar_reader(), 0);
    assert(std::get<EpochRef>(m.ref.ref_type) == EpochRef::tai);
    return true;
}

// ---------------------------------------------------------------------------
// Variable reference: integer column with code mapping
// ---------------------------------------------------------------------------
bool test_variable_ref_int_code() {
    MockTable table;
    table.scalars["TIME"][0] = CellValue{59000.5};
    table.scalars["TIME_REF"][0] = CellValue{std::int32_t(1)}; // code 1 -> TAI

    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.var_ref_column = "TIME_REF";
    desc.tab_ref_types = {"UTC", "TAI", "TDT"};
    desc.tab_ref_codes = {0, 1, 2};
    desc.units = {"d"};

    Measure m = read_scalar_measure(desc, table.make_scalar_reader(), 0);
    assert(std::get<EpochRef>(m.ref.ref_type) == EpochRef::tai);
    return true;
}

// ---------------------------------------------------------------------------
// Write scalar measure
// ---------------------------------------------------------------------------
bool test_write_scalar_epoch() {
    TableMeasDesc desc;
    desc.column_name = "TIME";
    desc.measure_type = MeasureType::epoch;
    desc.default_ref = EpochRef::utc;
    desc.units = {"d"};

    Measure m{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
              EpochValue{59000.5, 0.0}};

    std::vector<std::pair<std::size_t, CellValue>> written;
    ScalarCellWriter writer = [&](std::size_t col, std::uint64_t /*row*/, const CellValue& v) {
        written.emplace_back(col, v);
    };

    write_scalar_measure(desc, writer, 0, 0, m);
    assert(written.size() == 1);
    [[maybe_unused]] const auto* dp = std::get_if<double>(&written[0].second);
    assert(dp != nullptr);
    assert(near(*dp, 59000.5));
    return true;
}

// ---------------------------------------------------------------------------
// Write array measures
// ---------------------------------------------------------------------------
bool test_write_array_uvw() {
    TableMeasDesc desc;
    desc.column_name = "UVW";
    desc.measure_type = MeasureType::uvw;
    desc.default_ref = UvwRef::j2000;
    desc.units = {"m", "m", "m"};

    std::vector<Measure> measures = {
        {MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt}, UvwValue{10.0, 20.0, 30.0}},
        {MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt}, UvwValue{40.0, 50.0, 60.0}},
    };

    std::vector<double> written_data;
    auto write_fn = [&](const std::string& /*col*/, std::uint64_t /*row*/,
                        const std::vector<double>& values) { written_data = values; };

    write_array_measures(desc, write_fn, 0, measures);
    assert(written_data.size() == 6);
    assert(near(written_data[0], 10.0));
    assert(near(written_data[1], 20.0));
    assert(near(written_data[2], 30.0));
    assert(near(written_data[3], 40.0));
    assert(near(written_data[4], 50.0));
    assert(near(written_data[5], 60.0));
    return true;
}

// ---------------------------------------------------------------------------
// Array size not divisible by component count throws
// ---------------------------------------------------------------------------
bool test_array_bad_size_throws() {
    MockTable table;
    table.arrays["UVW"][0] = {100.0, 200.0}; // not divisible by 3

    TableMeasDesc desc;
    desc.column_name = "UVW";
    desc.measure_type = MeasureType::uvw;
    desc.default_ref = UvwRef::j2000;
    desc.units = {"m", "m", "m"};

    try {
        (void)read_array_measures(desc, table.make_array_reader(), table.make_scalar_reader(), 0);
        assert(false && "Should have thrown");
    } catch (const std::invalid_argument&) { // expected
    }
    return true;
}

// ---------------------------------------------------------------------------
// Read/write round-trip via mock
// ---------------------------------------------------------------------------
bool test_write_read_roundtrip() {
    // Write UVW measures into mock, then read back.
    TableMeasDesc desc;
    desc.column_name = "UVW";
    desc.measure_type = MeasureType::uvw;
    desc.default_ref = UvwRef::j2000;
    desc.units = {"m", "m", "m"};

    std::vector<Measure> original = {
        {MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt}, UvwValue{1.1, 2.2, 3.3}},
        {MeasureType::uvw, MeasureRef{UvwRef::j2000, std::nullopt}, UvwValue{4.4, 5.5, 6.6}},
    };

    MockTable table;
    auto write_fn = [&](const std::string& col, std::uint64_t row,
                        const std::vector<double>& values) { table.arrays[col][row] = values; };

    write_array_measures(desc, write_fn, 0, original);

    auto read_back =
        read_array_measures(desc, table.make_array_reader(), table.make_scalar_reader(), 0);

    assert(read_back.size() == 2);
    [[maybe_unused]] const auto& u0 = std::get<UvwValue>(read_back[0].value);
    assert(near(u0.u_m, 1.1));
    assert(near(u0.v_m, 2.2));
    assert(near(u0.w_m, 3.3));

    [[maybe_unused]] const auto& u1 = std::get<UvwValue>(read_back[1].value);
    assert(near(u1.u_m, 4.4));
    assert(near(u1.v_m, 5.5));
    assert(near(u1.w_m, 6.6));
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

    std::cout << "table_measure_column_test\n";
    run("read_scalar_epoch", test_read_scalar_epoch);
    run("read_scalar_frequency", test_read_scalar_frequency);
    run("read_array_uvw", test_read_array_uvw);
    run("read_array_direction", test_read_array_direction);
    run("variable_ref_string", test_variable_ref_string);
    run("variable_ref_int_code", test_variable_ref_int_code);
    run("write_scalar_epoch", test_write_scalar_epoch);
    run("write_array_uvw", test_write_array_uvw);
    run("array_bad_size_throws", test_array_bad_size_throws);
    run("write_read_roundtrip", test_write_read_roundtrip);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
