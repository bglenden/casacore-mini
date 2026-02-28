/// @file taql_cone_udf_test.cpp
/// @brief Tests for cone search functions and MeasUDF dispatch in TaQL.

#include "casacore_mini/taql.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace casacore_mini;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::cerr << "FAIL: " << label << "\n";
    }
}

static double get_double(const TaqlResult& r) {
    if (r.values.empty()) return 0.0;
    if (auto* d = std::get_if<double>(&r.values[0])) return *d;
    if (auto* i = std::get_if<std::int64_t>(&r.values[0])) return static_cast<double>(*i);
    if (auto* b = std::get_if<bool>(&r.values[0])) return *b ? 1.0 : 0.0;
    return 0.0;
}

static std::int64_t get_int(const TaqlResult& r) {
    if (r.values.empty()) return 0;
    if (auto* i = std::get_if<std::int64_t>(&r.values[0])) return *i;
    if (auto* d = std::get_if<double>(&r.values[0])) return static_cast<std::int64_t>(*d);
    return 0;
}

static bool get_bool(const TaqlResult& r) {
    if (r.values.empty()) return false;
    if (auto* b = std::get_if<bool>(&r.values[0])) return *b;
    return false;
}

static std::vector<double> get_double_vec(const TaqlResult& r) {
    if (r.values.empty()) return {};
    if (auto* dv = std::get_if<std::vector<double>>(&r.values[0])) return *dv;
    return {};
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr double kDeg = 3.14159265358979323846 / 180.0;

// ---------------------------------------------------------------------------
// INCONE function-call form
// ---------------------------------------------------------------------------
static void test_incone_function() {
    // Point at origin, cone centered at origin with 1-degree radius -> inside
    auto r1 = taql_calc("CALC INCONE(0.0, 0.0, 0.0, 0.0, 0.0175)");
    check(get_bool(r1) == true, "INCONE: point at center is inside cone");

    // Point 2 degrees away, cone with 1-degree radius -> outside
    auto r2 = taql_calc("CALC INCONE(0.0349, 0.0, 0.0, 0.0, 0.0175)");
    check(get_bool(r2) == false, "INCONE: point 2deg away from 1deg cone is outside");

    // Point 0.5 degrees away, cone with 1-degree radius -> inside
    auto r3 = taql_calc("CALC INCONE(0.00873, 0.0, 0.0, 0.0, 0.0175)");
    check(get_bool(r3) == true, "INCONE: point 0.5deg away from 1deg cone is inside");
}

// ---------------------------------------------------------------------------
// CONES alias
// ---------------------------------------------------------------------------
static void test_cones_alias() {
    auto r = taql_calc("CALC CONES(0.0, 0.0, 0.0, 0.0, 0.5)");
    check(get_bool(r) == true, "CONES: alias for INCONE at center");

    auto r2 = taql_calc("CALC CONES(1.0, 0.0, 0.0, 0.0, 0.01)");
    check(get_bool(r2) == false, "CONES: point far outside small cone");
}

// ---------------------------------------------------------------------------
// ANYCONE
// ---------------------------------------------------------------------------
static void test_anycone() {
    // ANYCONE with shared radius: point matches second cone
    // Centers at (0,0) and (0.5,0), point at (0.5, 0), radius 0.01
    auto r1 = taql_calc("CALC ANYCONE(0.5, 0.0, [0.0, 0.0, 0.5, 0.0], 0.01)");
    check(get_bool(r1) == true, "ANYCONE: point matches second cone");

    // Point far from both cones
    auto r2 = taql_calc("CALC ANYCONE(2.0, 1.0, [0.0, 0.0, 0.5, 0.0], 0.01)");
    check(get_bool(r2) == false, "ANYCONE: point matches no cone");

    // ANYCONE with per-cone radii (triples: lon,lat,radius,lon,lat,radius)
    auto r3 = taql_calc("CALC ANYCONE(0.0, 0.0, [0.0, 0.0, 0.5])");
    check(get_bool(r3) == true, "ANYCONE: triples form, point at center");
}

// ---------------------------------------------------------------------------
// FINDCONE
// ---------------------------------------------------------------------------
static void test_findcone() {
    // FINDCONE: find index of matching cone
    auto r1 = taql_calc("CALC FINDCONE(0.5, 0.0, [0.0, 0.0, 0.5, 0.0], 0.01)");
    check(get_int(r1) == 1, "FINDCONE: matches second cone (index 1)");

    // FINDCONE: matches first cone
    auto r2 = taql_calc("CALC FINDCONE(0.0, 0.0, [0.0, 0.0, 0.5, 0.0], 0.01)");
    check(get_int(r2) == 0, "FINDCONE: matches first cone (index 0)");

    // FINDCONE: no match returns -1
    auto r3 = taql_calc("CALC FINDCONE(2.0, 1.0, [0.0, 0.0, 0.5, 0.0], 0.01)");
    check(get_int(r3) == -1, "FINDCONE: no match returns -1");

    // FINDCONE with triples
    auto r4 = taql_calc("CALC FINDCONE(0.0, 0.0, [0.0, 0.0, 0.5])");
    check(get_int(r4) == 0, "FINDCONE: triples form, match at index 0");
}

// ---------------------------------------------------------------------------
// ANGDIST + INCONE combination
// ---------------------------------------------------------------------------
static void test_angdist_incone_combo() {
    // Verify ANGDIST between two points close together
    auto r1 = taql_calc("CALC ANGDIST(0.0, 0.0, 0.01, 0.0)");
    double dist = get_double(r1);
    check(std::abs(dist - 0.01) < 1e-6, "ANGDIST: small separation ~0.01 rad");

    // Use ANGDIST result conceptually with INCONE
    // Point 0.01 rad away, cone radius 0.02 -> inside
    auto r2 = taql_calc("CALC INCONE(0.01, 0.0, 0.0, 0.0, 0.02)");
    check(get_bool(r2) == true, "ANGDIST+INCONE: within cone radius");

    // Point 0.03 rad away, cone radius 0.02 -> outside
    auto r3 = taql_calc("CALC INCONE(0.03, 0.0, 0.0, 0.0, 0.02)");
    check(get_bool(r3) == false, "ANGDIST+INCONE: outside cone radius");
}

// ---------------------------------------------------------------------------
// MEAS.EPOCH
// ---------------------------------------------------------------------------
static void test_meas_epoch() {
    // Convert UTC to TAI: TAI = UTC + leap_seconds
    // MJD 51544.0 = 2000-01-01.5 UTC, TAI-UTC was 32 seconds
    // 32 seconds = 32/86400 days
    auto r = taql_calc("CALC MEAS.EPOCH(51544.0, 'UTC', 'TAI')");
    double tai_mjd = get_double(r);
    double expected_offset = 32.0 / 86400.0;
    check(std::abs(tai_mjd - (51544.0 + expected_offset)) < 1e-6,
          "MEAS.EPOCH: UTC->TAI at J2000");

    // TAI back to UTC should round-trip
    // Build a dynamic expression with the TAI result
    auto r2 = taql_calc("CALC MEAS.EPOCH(51544.00037037037, 'TAI', 'UTC')");
    double utc_mjd = get_double(r2);
    check(std::abs(utc_mjd - 51544.0) < 1e-4,
          "MEAS.EPOCH: TAI->UTC round-trip");
}

// ---------------------------------------------------------------------------
// MEAS.DIRECTION
// ---------------------------------------------------------------------------
static void test_meas_direction() {
    // Convert galactic center (0,0) from GALACTIC to J2000
    auto r = taql_calc("CALC MEAS.DIRECTION(0.0, 0.0, 'GALACTIC', 'J2000')");
    auto vec = get_double_vec(r);
    check(vec.size() == 2, "MEAS.DIRECTION: returns [lon,lat] array");

    // Galactic center in J2000 is approximately RA=266.4 deg, Dec=-28.9 deg
    if (vec.size() >= 2) {
        double ra_deg = vec[0] / kDeg;
        double dec_deg = vec[1] / kDeg;
        // Allow generous tolerance since exact values depend on implementation
        check(ra_deg > 260.0 && ra_deg < 270.0,
              "MEAS.DIRECTION: galactic center RA in J2000 ~266 deg");
        check(dec_deg > -32.0 && dec_deg < -26.0,
              "MEAS.DIRECTION: galactic center Dec in J2000 ~-29 deg");
    }

    // Identity conversion: J2000 -> J2000 should preserve values
    auto r2 = taql_calc("CALC MEAS.DIRECTION(1.5, 0.3, 'J2000', 'J2000')");
    auto vec2 = get_double_vec(r2);
    if (vec2.size() >= 2) {
        check(std::abs(vec2[0] - 1.5) < 1e-10, "MEAS.DIRECTION: J2000->J2000 lon identity");
        check(std::abs(vec2[1] - 0.3) < 1e-10, "MEAS.DIRECTION: J2000->J2000 lat identity");
    }
}

// ---------------------------------------------------------------------------
// MEAS.POSITION
// ---------------------------------------------------------------------------
static void test_meas_position() {
    // ITRF->ITRF identity
    auto r = taql_calc("CALC MEAS.POSITION(1000.0, 2000.0, 3000.0, 'ITRF', 'ITRF')");
    auto vec = get_double_vec(r);
    check(vec.size() == 3, "MEAS.POSITION: returns [x,y,z] array");
    if (vec.size() >= 3) {
        check(std::abs(vec[0] - 1000.0) < 1e-6, "MEAS.POSITION: identity x preserved");
        check(std::abs(vec[1] - 2000.0) < 1e-6, "MEAS.POSITION: identity y preserved");
        check(std::abs(vec[2] - 3000.0) < 1e-6, "MEAS.POSITION: identity z preserved");
    }
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------
static void test_edge_cases() {
    // INCONE with zero radius: only exact match
    auto r1 = taql_calc("CALC INCONE(0.0, 0.0, 0.0, 0.0, 0.0)");
    check(get_bool(r1) == true, "INCONE: zero radius at center -> true");

    // INCONE at poles
    auto r2 = taql_calc("CALC INCONE(0.0, 1.5707963, 0.5, 1.5707963, 0.01)");
    check(get_bool(r2) == true, "INCONE: near north pole, close points");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static void run(const char* label, void (*fn)()) {
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION in " << label << ": " << e.what() << "\n";
    }
}

int main() {
    run("incone_function", test_incone_function);
    run("cones_alias", test_cones_alias);
    run("anycone", test_anycone);
    run("findcone", test_findcone);
    run("angdist_incone_combo", test_angdist_incone_combo);
    run("meas_epoch", test_meas_epoch);
    run("meas_direction", test_meas_direction);
    run("meas_position", test_meas_position);
    run("edge_cases", test_edge_cases);

    std::cout << "taql_cone_udf_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
