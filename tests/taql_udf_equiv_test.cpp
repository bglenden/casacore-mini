// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file taql_udf_equiv_test.cpp
/// @brief P12-W7 tests: datetime, angle, array, complex, unit, cone, measure funcs.

#include "casacore_mini/table.hpp"
#include "casacore_mini/taql.hpp"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
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

static bool approx(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

// ---------------------------------------------------------------------------
// DateTime functions
// ---------------------------------------------------------------------------

static void test_datetime_year() {
    // MJD 51544.0 = 2000-01-01 (J2000 epoch)
    auto r = taql_calc("CALC YEAR(51544.0)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 2000, "YEAR(51544.0) = 2000");
}

static void test_datetime_month() {
    auto r = taql_calc("CALC MONTH(51544.0)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 1,
          "MONTH(51544.0) = 1 (January)");
}

static void test_datetime_day() {
    auto r = taql_calc("CALC DAY(51544.0)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 1, "DAY(51544.0) = 1");
}

static void test_datetime_cmonth() {
    auto r = taql_calc("CALC CMONTH(51544.0)");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "Jan",
          "CMONTH(51544.0) = Jan");
}

static void test_datetime_weekday() {
    auto r = taql_calc("CALC WEEKDAY(51544.0)");
    // 2000-01-01 was a Saturday = 6
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 6,
          "WEEKDAY(51544.0) = 6 (Saturday)");
}

static void test_datetime_cdow() {
    auto r = taql_calc("CALC CDOW(51544.0)");
    check(!r.values.empty() && std::get<std::string>(r.values[0]) == "Sat", "CDOW(51544.0) = Sat");
}

static void test_datetime_week() {
    auto r = taql_calc("CALC WEEK(51544.0)");
    check(!r.values.empty(), "WEEK returns a value");
}

static void test_datetime_mjdtodate() {
    auto r = taql_calc("CALC MJDTODATE(51544.0)");
    check(!r.values.empty(), "MJDTODATE returns a value");
    auto s = std::get<std::string>(r.values[0]);
    check(s.find("2000") != std::string::npos, "MJDTODATE contains 2000");
}

static void test_datetime_parse() {
    auto r = taql_calc("CALC DATETIME('2000/01/01')");
    check(!r.values.empty(), "DATETIME parse returns value");
    double mjd = std::get<double>(r.values[0]);
    check(approx(mjd, 51544.0, 1.0), "DATETIME('2000/01/01') ~ 51544");
}

static void test_datetime_date() {
    auto r = taql_calc("CALC DATE(51544.5)");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 51544.0),
          "DATE floors to day boundary");
}

static void test_datetime_time() {
    // MJD 51544.5 = noon = 43200 seconds into day
    auto r = taql_calc("CALC TIME(51544.5)");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 43200.0),
          "TIME(51544.5) = 43200.0 seconds");
}

// ---------------------------------------------------------------------------
// Angle functions
// ---------------------------------------------------------------------------

static void test_hms() {
    // 0 radians = 00:00:00
    auto r = taql_calc("CALC HMS(0.0)");
    check(!r.values.empty(), "HMS returns value");
    auto s = std::get<std::string>(r.values[0]);
    check(s.find("00:00:") != std::string::npos, "HMS(0) starts with 00:00:");
}

static void test_dms() {
    auto r = taql_calc("CALC DMS(0.0)");
    check(!r.values.empty(), "DMS returns value");
    auto s = std::get<std::string>(r.values[0]);
    check(s.find("00.00.") != std::string::npos, "DMS(0) starts with +00.00.");
}

static void test_normangle() {
    // -pi should normalize to ~pi
    auto r = taql_calc("CALC NORMANGLE(-3.14159265358979)");
    check(!r.values.empty(), "NORMANGLE returns value");
    double a = std::get<double>(r.values[0]);
    check(a > 3.0 && a < 3.2, "NORMANGLE(-pi) ~ pi");
}

static void test_angdist() {
    // Distance from (0,0) to (0,pi/2) = pi/2
    auto r = taql_calc("CALC ANGDIST(0.0, 0.0, 0.0, 1.5707963)");
    check(!r.values.empty(), "ANGDIST returns value");
    double d = std::get<double>(r.values[0]);
    check(approx(d, 1.5707963, 1e-5), "ANGDIST(0,0,0,pi/2) = pi/2");
}

// ---------------------------------------------------------------------------
// Array functions
// ---------------------------------------------------------------------------

static void test_ndim_nelem() {
    auto r = taql_calc("CALC NDIM(42.0)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 0, "NDIM of scalar = 0");

    r = taql_calc("CALC NELEM(42.0)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 1, "NELEM of scalar = 1");
}

static void test_array_create() {
    auto r = taql_calc("CALC ARRAY(1.0, 5)");
    check(!r.values.empty(), "ARRAY returns value");
    auto* vec = std::get_if<std::vector<double>>(&r.values[0]);
    check(vec != nullptr && vec->size() == 5, "ARRAY(1.0, 5) has 5 elements");
    if (vec)
        check((*vec)[0] == 1.0 && (*vec)[4] == 1.0, "ARRAY filled with 1.0");
}

static void test_arrsum() {
    // Create a table with an array column to test array reduction
    auto dir = fs::temp_directory_path() / "taql_arrsum_test";
    fs::remove_all(dir);
    std::vector<TableColumnSpec> cols = {
        {"VALUES", DataType::tp_double, ColumnKind::scalar, {}, ""},
    };
    auto tbl = Table::create(dir, cols, 3);
    tbl.write_scalar_cell("VALUES", 0, CellValue(10.0));
    tbl.write_scalar_cell("VALUES", 1, CellValue(20.0));
    tbl.write_scalar_cell("VALUES", 2, CellValue(30.0));
    tbl.flush();

    // Test ARRSUM via CALC on a vector literal
    auto r = taql_calc("CALC ARRSUM(ARRAY(3.0, 4))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 12.0),
          "ARRSUM([3,3,3,3]) = 12");

    fs::remove_all(dir);
}

static void test_arrmin_arrmax() {
    auto rmin = taql_calc("CALC ARRMIN(ARRAY(5.0, 3))");
    check(!rmin.values.empty() && approx(std::get<double>(rmin.values[0]), 5.0),
          "ARRMIN of constant array");

    auto rmax = taql_calc("CALC ARRMAX(ARRAY(5.0, 3))");
    check(!rmax.values.empty() && approx(std::get<double>(rmax.values[0]), 5.0),
          "ARRMAX of constant array");
}

static void test_arrmean() {
    auto r = taql_calc("CALC ARRMEAN(ARRAY(10.0, 4))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 10.0),
          "ARRMEAN of constant array");
}

static void test_arrmedian() {
    auto r = taql_calc("CALC ARRMEDIAN(ARRAY(5.0, 3))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 5.0),
          "ARRMEDIAN of constant array");
}

// ---------------------------------------------------------------------------
// Complex arithmetic
// ---------------------------------------------------------------------------

static void test_complex_funcs() {
    auto r = taql_calc("CALC COMPLEX(3.0, 4.0)");
    check(!r.values.empty(), "COMPLEX constructor returns value");
    auto* cz = std::get_if<std::complex<double>>(&r.values[0]);
    check(cz != nullptr, "COMPLEX returns complex type");
    if (cz) {
        check(approx(cz->real(), 3.0), "COMPLEX real part = 3");
        check(approx(cz->imag(), 4.0), "COMPLEX imag part = 4");
    }
}

static void test_complex_norm() {
    auto r = taql_calc("CALC NORM(COMPLEX(3.0, 4.0))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 25.0), "NORM(3+4i) = 25");
}

static void test_complex_arg() {
    auto r = taql_calc("CALC ARG(COMPLEX(1.0, 1.0))");
    check(!r.values.empty(), "ARG returns value");
    double a = std::get<double>(r.values[0]);
    check(approx(a, 0.7853981, 1e-5), "ARG(1+1i) = pi/4");
}

static void test_complex_conj() {
    auto r = taql_calc("CALC CONJ(COMPLEX(3.0, 4.0))");
    check(!r.values.empty(), "CONJ returns value");
    auto* cz = std::get_if<std::complex<double>>(&r.values[0]);
    check(cz != nullptr && approx(cz->imag(), -4.0), "CONJ(3+4i) imag = -4");
}

static void test_complex_real_imag() {
    auto r = taql_calc("CALC REAL(COMPLEX(3.0, 4.0))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 3.0), "REAL(3+4i) = 3");

    r = taql_calc("CALC IMAG(COMPLEX(3.0, 4.0))");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 4.0), "IMAG(3+4i) = 4");
}

// ---------------------------------------------------------------------------
// Unit suffix conversion
// ---------------------------------------------------------------------------

static void test_unit_deg_to_rad() {
    // 180 deg should be approximately pi radians
    auto r = taql_calc("CALC 180.0deg");
    check(!r.values.empty(), "unit suffix returns value");
    double val = std::get<double>(r.values[0]);
    check(approx(val, 3.14159265, 1e-6), "180deg = pi radians");
}

static void test_unit_arcsec() {
    // 3600 arcsec = 1 degree = pi/180 radians
    auto r = taql_calc("CALC 3600.0arcsec");
    check(!r.values.empty(), "arcsec unit returns value");
    double val = std::get<double>(r.values[0]);
    check(approx(val, 3.14159265 / 180.0, 1e-8), "3600arcsec = 1deg in rad");
}

static void test_unit_km() {
    auto r = taql_calc("CALC 1.0km");
    check(!r.values.empty() && approx(std::get<double>(r.values[0]), 1000.0), "1km = 1000m");
}

// ---------------------------------------------------------------------------
// Cone search
// ---------------------------------------------------------------------------

static void test_incone_func() {
    // Point at (0,0), cone center at (0,0) with radius 0.1: should be inside
    auto r = taql_calc("CALC INCONE(0.0, 0.0, 0.0, 0.0, 0.1)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == true, "INCONE at center is true");

    // Point at (1.0, 0), cone center at (0,0) with radius 0.1: should be outside
    r = taql_calc("CALC INCONE(1.0, 0.0, 0.0, 0.0, 0.1)");
    check(!r.values.empty() && std::get<bool>(r.values[0]) == false,
          "INCONE far from center is false");
}

static void test_findcone_func() {
    auto r = taql_calc("CALC FINDCONE(0.0, 0.0, 0.0, 0.0, 0.1)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == 0,
          "FINDCONE inside returns 0");

    r = taql_calc("CALC FINDCONE(1.0, 0.0, 0.0, 0.0, 0.1)");
    check(!r.values.empty() && std::get<std::int64_t>(r.values[0]) == -1,
          "FINDCONE outside returns -1");
}

// ---------------------------------------------------------------------------
// MeasUDF equivalents (built-in measure conversion)
// ---------------------------------------------------------------------------

static void test_meas_dir_j2000_identity() {
    // J2000 to J2000 should be identity
    auto r = taql_calc("CALC MEAS_DIR_J2000(1.0, 0.5)");
    check(!r.values.empty(), "MEAS_DIR_J2000 returns value");
    auto* vec = std::get_if<std::vector<double>>(&r.values[0]);
    check(vec != nullptr && vec->size() == 2, "MEAS.DIR returns 2-element vector");
    if (vec) {
        check(approx((*vec)[0], 1.0, 1e-9), "J2000->J2000 lon preserved");
        check(approx((*vec)[1], 0.5, 1e-9), "J2000->J2000 lat preserved");
    }
}

static void test_meas_dir_galactic() {
    // Convert J2000 galactic center direction to galactic — should be near (0,0)
    // Sgr A* is at J2000: (RA=266.4168deg, Dec=-29.0078deg)
    double ra_rad = 266.4168 * 3.14159265358979 / 180.0;
    double dec_rad = -29.0078 * 3.14159265358979 / 180.0;
    auto r = taql_calc("CALC MEAS_DIR_GALACTIC(" + std::to_string(ra_rad) + ", " +
                       std::to_string(dec_rad) + ")");
    check(!r.values.empty(), "MEAS_DIR_GALACTIC returns value");
    auto* vec = std::get_if<std::vector<double>>(&r.values[0]);
    check(vec != nullptr && vec->size() == 2, "MEAS_DIR_GAL returns 2-element vector");
    // Near galactic center: lon should be near 0 or 2*pi, lat near 0
    if (vec) {
        check(std::abs((*vec)[1]) < 0.2, "Galactic lat near 0 for Sgr A* direction");
    }
}

static void test_meas_epoch_tai() {
    // UTC to TAI adds ~37 seconds (varies by epoch, but always adds leap seconds).
    // For MJD 51544.0, the TAI-UTC offset is 32 seconds (in 2000).
    auto r = taql_calc("CALC MEAS_EPOCH_TAI(51544.0)");
    check(!r.values.empty(), "MEAS_EPOCH_TAI returns value");
    double tai_mjd = std::get<double>(r.values[0]);
    // TAI should be slightly larger than UTC (by leap seconds / 86400)
    check(tai_mjd > 51544.0, "TAI > UTC");
    double diff_seconds = (tai_mjd - 51544.0) * 86400.0;
    check(diff_seconds > 20 && diff_seconds < 50, "TAI-UTC offset is reasonable (20-50 seconds)");
}

// ---------------------------------------------------------------------------
// Pattern functions
// ---------------------------------------------------------------------------

static void test_pattern_func() {
    auto r = taql_calc("CALC PATTERN('hello*')");
    check(!r.values.empty(), "PATTERN returns value");
    auto s = std::get<std::string>(r.values[0]);
    check(s.find(".*") != std::string::npos, "PATTERN converts * to .*");
}

static void test_regex_func() {
    auto r = taql_calc("CALC REGEX('[a-z]+')");
    check(!r.values.empty(), "REGEX returns value");
    check(std::get<std::string>(r.values[0]) == "[a-z]+", "REGEX passes through");
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

static void run(void (*fn)(), const char* name) {
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION in " << name << ": " << e.what() << "\n";
    }
}

int main() {
    // DateTime
    run(test_datetime_year, "test_datetime_year");
    run(test_datetime_month, "test_datetime_month");
    run(test_datetime_day, "test_datetime_day");
    run(test_datetime_cmonth, "test_datetime_cmonth");
    run(test_datetime_weekday, "test_datetime_weekday");
    run(test_datetime_cdow, "test_datetime_cdow");
    run(test_datetime_week, "test_datetime_week");
    run(test_datetime_mjdtodate, "test_datetime_mjdtodate");
    run(test_datetime_parse, "test_datetime_parse");
    run(test_datetime_date, "test_datetime_date");
    run(test_datetime_time, "test_datetime_time");

    // Angles
    run(test_hms, "test_hms");
    run(test_dms, "test_dms");
    run(test_normangle, "test_normangle");
    run(test_angdist, "test_angdist");

    // Arrays
    run(test_ndim_nelem, "test_ndim_nelem");
    run(test_array_create, "test_array_create");
    run(test_arrsum, "test_arrsum");
    run(test_arrmin_arrmax, "test_arrmin_arrmax");
    run(test_arrmean, "test_arrmean");
    run(test_arrmedian, "test_arrmedian");

    // Complex
    run(test_complex_funcs, "test_complex_funcs");
    run(test_complex_norm, "test_complex_norm");
    run(test_complex_arg, "test_complex_arg");
    run(test_complex_conj, "test_complex_conj");
    run(test_complex_real_imag, "test_complex_real_imag");

    // Units
    run(test_unit_deg_to_rad, "test_unit_deg_to_rad");
    run(test_unit_arcsec, "test_unit_arcsec");
    run(test_unit_km, "test_unit_km");

    // Cone search
    run(test_incone_func, "test_incone_func");
    run(test_findcone_func, "test_findcone_func");

    // MeasUDF
    run(test_meas_dir_j2000_identity, "test_meas_dir_j2000_identity");
    run(test_meas_dir_galactic, "test_meas_dir_galactic");
    run(test_meas_epoch_tai, "test_meas_epoch_tai");

    // Pattern functions
    run(test_pattern_func, "test_pattern_func");
    run(test_regex_func, "test_regex_func");

    std::cout << "taql_udf_equiv_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
