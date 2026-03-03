// SPDX-FileCopyrightText: 2026 Brian Glendenning
// SPDX-License-Identifier: LGPL-3.0-or-later

/// @file taql_eval_test.cpp
/// @brief Tests for TaQL expression evaluator and CALC.

#include "casacore_mini/taql.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

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

static std::string get_string(const TaqlResult& r) {
    if (r.values.empty()) return {};
    if (auto* s = std::get_if<std::string>(&r.values[0])) return *s;
    return {};
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------

static bool test_int_arithmetic() {
    check(get_int(taql_calc("CALC 1 + 2")) == 3, "1+2=3");
    check(get_int(taql_calc("CALC 10 - 3")) == 7, "10-3=7");
    check(get_int(taql_calc("CALC 4 * 5")) == 20, "4*5=20");
    check(get_int(taql_calc("CALC 10 / 3")) == 3, "10/3=3 (int div)");
    check(get_int(taql_calc("CALC 10 % 3")) == 1, "10%3=1");
    check(get_int(taql_calc("CALC 2 ** 10")) == 1024, "2**10=1024");
    return g_fail == 0;
}

static bool test_float_arithmetic() {
    auto v = get_double(taql_calc("CALC 1.5 + 2.5"));
    check(std::abs(v - 4.0) < 1e-10, "1.5+2.5=4.0");

    v = get_double(taql_calc("CALC 3.14 * 2.0"));
    check(std::abs(v - 6.28) < 1e-10, "3.14*2.0=6.28");

    v = get_double(taql_calc("CALC 10.0 / 3.0"));
    check(std::abs(v - 10.0 / 3.0) < 1e-10, "10.0/3.0");
    return g_fail == 0;
}

static bool test_unary() {
    check(get_int(taql_calc("CALC -5")) == -5, "-5");
    check(get_double(taql_calc("CALC -3.14")) < -3.13, "-3.14");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

static bool test_comparisons() {
    check(get_bool(taql_calc("CALC 1 = 1")), "1=1");
    check(!get_bool(taql_calc("CALC 1 = 2")), "1!=2");
    check(get_bool(taql_calc("CALC 1 < 2")), "1<2");
    check(get_bool(taql_calc("CALC 2 > 1")), "2>1");
    check(get_bool(taql_calc("CALC 2 <= 2")), "2<=2");
    check(get_bool(taql_calc("CALC 2 >= 2")), "2>=2");
    check(get_bool(taql_calc("CALC 1 != 2")), "1!=2");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Logical
// ---------------------------------------------------------------------------

static bool test_logical() {
    check(get_bool(taql_calc("CALC TRUE AND TRUE")), "T AND T");
    check(!get_bool(taql_calc("CALC TRUE AND FALSE")), "T AND F");
    check(get_bool(taql_calc("CALC TRUE OR FALSE")), "T OR F");
    check(!get_bool(taql_calc("CALC NOT TRUE")), "NOT T");
    check(get_bool(taql_calc("CALC NOT FALSE")), "NOT F");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Math functions
// ---------------------------------------------------------------------------

static bool test_math_functions() {
    auto v = get_double(taql_calc("CALC sin(0)"));
    check(std::abs(v) < 1e-10, "sin(0)=0");

    v = get_double(taql_calc("CALC cos(0)"));
    check(std::abs(v - 1.0) < 1e-10, "cos(0)=1");

    v = get_double(taql_calc("CALC sqrt(4)"));
    check(std::abs(v - 2.0) < 1e-10, "sqrt(4)=2");

    v = get_double(taql_calc("CALC abs(-5)"));
    check(std::abs(v - 5.0) < 1e-10, "abs(-5)=5");

    v = get_double(taql_calc("CALC floor(3.7)"));
    check(std::abs(v - 3.0) < 1e-10, "floor(3.7)=3");

    v = get_double(taql_calc("CALC ceil(3.2)"));
    check(std::abs(v - 4.0) < 1e-10, "ceil(3.2)=4");

    v = get_double(taql_calc("CALC round(3.5)"));
    check(std::abs(v - 4.0) < 1e-10, "round(3.5)=4");

    v = get_double(taql_calc("CALC log(1)"));
    check(std::abs(v) < 1e-10, "log(1)=0");

    v = get_double(taql_calc("CALC exp(0)"));
    check(std::abs(v - 1.0) < 1e-10, "exp(0)=1");

    v = get_double(taql_calc("CALC pow(2, 8)"));
    check(std::abs(v - 256.0) < 1e-10, "pow(2,8)=256");

    v = get_double(taql_calc("CALC atan2(1, 1)"));
    check(std::abs(v - 0.785398163) < 1e-6, "atan2(1,1)=pi/4");

    v = get_double(taql_calc("CALC min(3, 7)"));
    check(std::abs(v - 3.0) < 1e-10, "min(3,7)=3");

    v = get_double(taql_calc("CALC max(3, 7)"));
    check(std::abs(v - 7.0) < 1e-10, "max(3,7)=7");

    v = get_double(taql_calc("CALC square(5)"));
    check(std::abs(v - 25.0) < 1e-10, "square(5)=25");

    v = get_double(taql_calc("CALC cube(3)"));
    check(std::abs(v - 27.0) < 1e-10, "cube(3)=27");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

static bool test_constants() {
    auto v = get_double(taql_calc("CALC pi()"));
    check(std::abs(v - 3.14159265358979) < 1e-10, "pi()");

    v = get_double(taql_calc("CALC e()"));
    check(std::abs(v - 2.71828182845904) < 1e-10, "e()");

    v = get_double(taql_calc("CALC c()"));
    check(std::abs(v - 299792458.0) < 1.0, "c()");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// IIF
// ---------------------------------------------------------------------------

static bool test_iif() {
    check(get_int(taql_calc("CALC IIF(TRUE, 10, 20)")) == 10, "IIF true");
    check(get_int(taql_calc("CALC IIF(FALSE, 10, 20)")) == 20, "IIF false");
    check(get_int(taql_calc("CALC IIF(1 > 0, 42, 0)")) == 42, "IIF expr");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// String functions
// ---------------------------------------------------------------------------

static bool test_string_functions() {
    auto s = get_string(taql_calc("CALC upcase('hello')"));
    check(s == "HELLO", "upcase");

    s = get_string(taql_calc("CALC downcase('WORLD')"));
    check(s == "world", "downcase");

    auto v = get_int(taql_calc("CALC strlen('test')"));
    check(v == 4, "strlen");

    s = get_string(taql_calc("CALC trim('  hi  ')"));
    check(s == "hi", "trim");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Complex expressions
// ---------------------------------------------------------------------------

static bool test_complex_expr() {
    auto v = get_double(taql_calc("CALC (1 + 2) * (3 + 4)"));
    check(std::abs(v - 21.0) < 1e-10, "(1+2)*(3+4)=21");

    v = get_double(taql_calc("CALC sin(0) + cos(0)"));
    check(std::abs(v - 1.0) < 1e-10, "sin(0)+cos(0)=1");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// SHOW command
// ---------------------------------------------------------------------------

static bool test_show_via_calc() {
    auto r = taql_calc("SHOW functions");
    check(!r.show_text.empty(), "SHOW functions produces text");
    return g_fail == 0;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

static void run(const char* name, bool (*fn)()) {
    try {
        fn();
    } catch (const std::exception& e) {
        ++g_fail;
        std::cerr << "EXCEPTION in " << name << ": " << e.what() << "\n";
    }
}

int main() {
    run("int_arithmetic", test_int_arithmetic);
    run("float_arithmetic", test_float_arithmetic);
    run("unary", test_unary);
    run("comparisons", test_comparisons);
    run("logical", test_logical);
    run("math_functions", test_math_functions);
    run("constants", test_constants);
    run("iif", test_iif);
    run("string_functions", test_string_functions);
    run("complex_expr", test_complex_expr);
    run("show_via_calc", test_show_via_calc);

    std::cout << "taql_eval_test: " << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
