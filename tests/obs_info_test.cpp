#include "casacore_mini/measure_types.hpp"
#include "casacore_mini/obs_info.hpp"
#include "casacore_mini/record.hpp"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace casacore_mini;

constexpr double kTol = 1.0e-12;

[[maybe_unused]] bool near(double a, double b) {
    return std::abs(a - b) < kTol * std::max(1.0, std::abs(a));
}

// ---------------------------------------------------------------------------
// Basic round-trip
// ---------------------------------------------------------------------------
bool test_basic_roundtrip() {
    ObsInfo info;
    info.telescope = "VLA";
    info.observer = "TestUser";

    Record rec;
    obs_info_to_record(info, rec);

    ObsInfo restored = obs_info_from_record(rec);
    assert(restored.telescope == "VLA");
    assert(restored.observer == "TestUser");
    assert(!restored.obs_date.has_value());
    assert(!restored.telescope_position.has_value());
    assert(!restored.pointing_center.has_value());
    return true;
}

// ---------------------------------------------------------------------------
// With observation date
// ---------------------------------------------------------------------------
bool test_with_obsdate() {
    ObsInfo info;
    info.telescope = "ALMA";
    info.observer = "Astronomer";
    info.obs_date = Measure{MeasureType::epoch, MeasureRef{EpochRef::utc, std::nullopt},
                            EpochValue{59000.0, 0.5}};

    Record rec;
    obs_info_to_record(info, rec);

    ObsInfo restored = obs_info_from_record(rec);
    assert(restored.obs_date.has_value());
    assert(restored.obs_date->type == MeasureType::epoch);
    [[maybe_unused]] const auto& ev = std::get<EpochValue>(restored.obs_date->value);
    assert(near(ev.day, 59000.0));
    assert(near(ev.fraction, 0.5));
    return true;
}

// ---------------------------------------------------------------------------
// With telescope position
// ---------------------------------------------------------------------------
bool test_with_telescope_position() {
    ObsInfo info;
    info.telescope = "GBT";
    info.telescope_position =
        Measure{MeasureType::position, MeasureRef{PositionRef::itrf, std::nullopt},
                PositionValue{882589.0, -4924872.0, 3943729.0}};

    Record rec;
    obs_info_to_record(info, rec);

    ObsInfo restored = obs_info_from_record(rec);
    assert(restored.telescope_position.has_value());
    assert(restored.telescope_position->type == MeasureType::position);
    [[maybe_unused]] const auto& pv = std::get<PositionValue>(restored.telescope_position->value);
    assert(near(pv.x_m, 882589.0));
    return true;
}

// ---------------------------------------------------------------------------
// With pointing center
// ---------------------------------------------------------------------------
bool test_with_pointing_center() {
    ObsInfo info;
    info.telescope = "VLA";
    info.pointing_center = {3.14, -0.5};

    Record rec;
    obs_info_to_record(info, rec);

    ObsInfo restored = obs_info_from_record(rec);
    assert(restored.pointing_center.has_value());
    assert(near(restored.pointing_center->first, 3.14));
    assert(near(restored.pointing_center->second, -0.5));
    return true;
}

// ---------------------------------------------------------------------------
// Empty Record returns empty ObsInfo
// ---------------------------------------------------------------------------
bool test_empty_record() {
    Record rec;
    ObsInfo restored = obs_info_from_record(rec);
    assert(restored.telescope.empty());
    assert(restored.observer.empty());
    assert(!restored.obs_date.has_value());
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

    std::cout << "obs_info_test\n";
    run("basic_roundtrip", test_basic_roundtrip);
    run("with_obsdate", test_with_obsdate);
    run("with_telescope_position", test_with_telescope_position);
    run("with_pointing_center", test_with_pointing_center);
    run("empty_record", test_empty_record);

    if (failures > 0) {
        std::cout << failures << " test(s) FAILED\n";
        return EXIT_FAILURE;
    }
    std::cout << "All tests passed.\n";
    return EXIT_SUCCESS;
}
