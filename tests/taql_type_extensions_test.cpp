#include "casacore_mini/taql.hpp"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace casacore_mini;

static int check_count = 0;

static void check(bool condition, const char* msg) {
    ++check_count;
    if (!condition) {
        std::cerr << "FAIL: " << msg << "\n";
        std::abort();
    }
}

static double as_dbl(const TaqlValue& v) {
    if (auto* d = std::get_if<double>(&v)) return *d;
    if (auto* i = std::get_if<std::int64_t>(&v)) return static_cast<double>(*i);
    throw std::runtime_error("not a number");
}

static std::int64_t as_i64(const TaqlValue& v) {
    if (auto* i = std::get_if<std::int64_t>(&v)) return *i;
    throw std::runtime_error("not an int64");
}

static std::string as_str(const TaqlValue& v) {
    if (auto* s = std::get_if<std::string>(&v)) return *s;
    throw std::runtime_error("not a string");
}

static std::complex<double> as_cpx(const TaqlValue& v) {
    if (auto* c = std::get_if<std::complex<double>>(&v)) return *c;
    throw std::runtime_error("not complex");
}

// ---- Complex arithmetic ----
static void test_complex_arithmetic() {
    {
        auto r = taql_calc("CALC COMPLEX(3, 4)");
        auto c = as_cpx(r.values[0]);
        check(std::abs(c.real() - 3.0) < 1e-10, "complex real");
        check(std::abs(c.imag() - 4.0) < 1e-10, "complex imag");
    }
    {
        auto r = taql_calc("CALC REAL(COMPLEX(3, 4))");
        check(std::abs(as_dbl(r.values[0]) - 3.0) < 1e-10, "real of complex");
    }
    {
        auto r = taql_calc("CALC IMAG(COMPLEX(3, 4))");
        check(std::abs(as_dbl(r.values[0]) - 4.0) < 1e-10, "imag of complex");
    }
    {
        auto r = taql_calc("CALC NORM(COMPLEX(3, 4))");
        // norm = 3^2 + 4^2 = 25
        check(std::abs(as_dbl(r.values[0]) - 25.0) < 1e-10, "norm of complex");
    }
    {
        auto r = taql_calc("CALC ARG(COMPLEX(0, 1))");
        // arg of i = pi/2
        check(std::abs(as_dbl(r.values[0]) - 3.14159265358979323846 / 2.0) < 1e-10, "arg of i");
    }
    {
        auto r = taql_calc("CALC CONJ(COMPLEX(3, 4))");
        auto c = as_cpx(r.values[0]);
        check(std::abs(c.real() - 3.0) < 1e-10 && std::abs(c.imag() + 4.0) < 1e-10, "conj");
    }
    {
        // Complex addition via binary ops
        auto r = taql_calc("CALC COMPLEX(1, 2) + COMPLEX(3, 4)");
        auto c = as_cpx(r.values[0]);
        check(std::abs(c.real() - 4.0) < 1e-10 && std::abs(c.imag() - 6.0) < 1e-10,
              "complex addition");
    }
    {
        auto r = taql_calc("CALC COMPLEX(1, 2) * COMPLEX(3, 4)");
        auto c = as_cpx(r.values[0]);
        // (1+2i)*(3+4i) = 3+4i+6i+8i^2 = -5+10i
        check(std::abs(c.real() + 5.0) < 1e-10 && std::abs(c.imag() - 10.0) < 1e-10,
              "complex multiplication");
    }
    {
        auto r = taql_calc("CALC COMPLEX(1, 0) / COMPLEX(0, 1)");
        auto c = as_cpx(r.values[0]);
        // 1/i = -i
        check(std::abs(c.real()) < 1e-10 && std::abs(c.imag() + 1.0) < 1e-10,
              "complex division");
    }
    // Real function on non-complex should return the value
    {
        auto r = taql_calc("CALC REAL(42)");
        check(std::abs(as_dbl(r.values[0]) - 42.0) < 1e-10, "real of scalar");
    }
    {
        auto r = taql_calc("CALC IMAG(42)");
        check(std::abs(as_dbl(r.values[0])) < 1e-10, "imag of scalar");
    }
}

// ---- DateTime functions ----
static void test_datetime_functions() {
    {
        // 2000-01-01 = MJD 51544
        auto r = taql_calc("CALC DATETIME('2000/01/01')");
        check(std::abs(as_dbl(r.values[0]) - 51544.0) < 0.5, "datetime 2000/01/01");
    }
    {
        auto r = taql_calc("CALC YEAR(DATETIME('2024/06/15'))");
        check(as_i64(r.values[0]) == 2024, "year 2024");
    }
    {
        auto r = taql_calc("CALC MONTH(DATETIME('2024/06/15'))");
        check(as_i64(r.values[0]) == 6, "month 6");
    }
    {
        auto r = taql_calc("CALC DAY(DATETIME('2024/06/15'))");
        check(as_i64(r.values[0]) == 15, "day 15");
    }
    {
        auto r = taql_calc("CALC CMONTH(DATETIME('2024/03/15'))");
        check(as_str(r.values[0]) == "Mar", "cmonth Mar");
    }
    {
        // MJD to date
        auto r = taql_calc("CALC MJDTODATE(51544.0)");
        auto s = as_str(r.values[0]);
        check(s.substr(0, 10) == "2000/01/01", "mjdtodate 51544");
    }
    {
        auto r = taql_calc("CALC DATE(51544.0)");
        check(as_str(r.values[0]) == "2000/01/01", "date of mjd 51544");
    }
    {
        auto r = taql_calc("CALC WEEKDAY(DATETIME('2024/01/01'))");
        // 2024/01/01 is Monday = 1
        check(as_i64(r.values[0]) == 1, "weekday Monday");
    }
    {
        auto r = taql_calc("CALC CDOW(DATETIME('2024/01/01'))");
        check(as_str(r.values[0]) == "Mon", "cdow Mon");
    }
    {
        auto r = taql_calc("CALC WEEK(DATETIME('2024/01/15'))");
        check(as_i64(r.values[0]) == 3, "week ~3 for Jan 15");
    }
    {
        auto r = taql_calc("CALC CTOD('2024/06/15')");
        auto mjd = as_dbl(r.values[0]);
        auto r2 = taql_calc("CALC YEAR(CTOD('2024/06/15'))");
        check(as_i64(r2.values[0]) == 2024, "ctod roundtrip year");
        (void)mjd;
    }
    {
        // ISO format
        auto r = taql_calc("CALC DATETIME('2024-06-15T12:30:00')");
        auto mjd = as_dbl(r.values[0]);
        // Should be around MJD 60476.52
        check(mjd > 60475 && mjd < 60478, "datetime ISO format");
    }
}

// ---- Angle functions ----
static void test_angle_functions() {
    {
        // HMS of 0 radians
        auto r = taql_calc("CALC HMS(0.0)");
        auto s = as_str(r.values[0]);
        check(s.substr(0, 5) == "00:00", "hms of 0 rad");
    }
    {
        // DMS of 0 radians
        auto r = taql_calc("CALC DMS(0.0)");
        auto s = as_str(r.values[0]);
        check(s.find("+00") != std::string::npos, "dms of 0 rad");
    }
    {
        // NORMANGLE
        constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
        auto r = taql_calc("CALC NORMANGLE(-1.0)");
        auto val = as_dbl(r.values[0]);
        check(val >= 0.0 && val < kTwoPi, "normangle positive");
    }
    {
        // ANGDIST between same point = 0
        auto r = taql_calc("CALC ANGDIST(0.0, 0.0, 0.0, 0.0)");
        check(std::abs(as_dbl(r.values[0])) < 1e-10, "angdist same point");
    }
    {
        // ANGDIST between north pole and equator = pi/2
        auto r = taql_calc("CALC ANGDIST(0.0, 0.0, 0.0, 1.5707963267948966)");
        check(std::abs(as_dbl(r.values[0]) - 1.5707963267948966) < 1e-6, "angdist pole-equator");
    }
    {
        // HMS of pi/2 radians = 6h
        auto r = taql_calc("CALC HMS(1.5707963267948966)");
        auto s = as_str(r.values[0]);
        check(s.substr(0, 2) == "06", "hms of pi/2 = 6h");
    }
}

// ---- Pattern functions ----
static void test_pattern_functions() {
    {
        auto r = taql_calc("CALC REGEX('hello')");
        check(as_str(r.values[0]) == "hello", "regex passthrough");
    }
    {
        auto r = taql_calc("CALC PATTERN('test*')");
        check(as_str(r.values[0]) == "test*", "pattern passthrough");
    }
}

// ---- Unit conversion ----
static void test_unit_conversion() {
    {
        // UNIT(180, 'deg', 'rad') = pi
        auto r = taql_calc("CALC UNIT(180.0, 'deg', 'rad')");
        check(std::abs(as_dbl(r.values[0]) - 3.14159265358979323846) < 1e-6, "deg to rad");
    }
    {
        // UNIT(1, 'km', 'm') = 1000
        auto r = taql_calc("CALC UNIT(1.0, 'km', 'm')");
        check(std::abs(as_dbl(r.values[0]) - 1000.0) < 1e-6, "km to m");
    }
}

int main() { // NOLINT(bugprone-exception-escape)
    test_complex_arithmetic();
    test_datetime_functions();
    test_angle_functions();
    test_pattern_functions();
    test_unit_conversion();

    std::cout << "taql_type_extensions_test: " << check_count << " checks passed\n";
    return 0;
}
