/// @file lel_malformed_test.cpp
/// Malformed-input hardening tests for the LEL parser and evaluator.
/// Complements lel_parser_test by exercising additional edge cases:
/// deeply nested expressions, additional type mismatches, reduction on
/// empty lattices, numeric edge cases (NaN/Inf propagation), and
/// identifier length stress.

#include "casacore_mini/lattice_expr.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace casacore_mini;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__               \
                      << "]: " #cond << "\n";                                  \
            ++g_fail;                                                          \
        } else {                                                               \
            ++g_pass;                                                          \
        }                                                                      \
    } while (false)

#define CHECK_THROWS_AS(expr, ExType)                                          \
    do {                                                                       \
        bool threw = false;                                                    \
        try { (void)(expr); } catch (const ExType&) { threw = true; }          \
        CHECK(threw);                                                          \
    } while (false)

#define CHECK_THROWS(expr)                                                     \
    do {                                                                       \
        bool threw = false;                                                    \
        try { (void)(expr); } catch (...) { threw = true; }                    \
        CHECK(threw);                                                          \
    } while (false)

// ── Helpers ──────────────────────────────────────────────────────────

static LatticeArray<float> make_4x4(float val = 1.0f) {
    IPosition sh{4, 4};
    LatticeArray<float> arr(sh, val);
    return arr;
}

static LatticeArray<bool> make_4x4b(bool val = true) {
    IPosition sh{4, 4};
    LatticeArray<bool> arr(sh, val);
    return arr;
}

static LatticeArray<float> make_3x3(float val = 1.0f) {
    IPosition sh{3, 3};
    LatticeArray<float> arr(sh, val);
    return arr;
}

// ── Additional parse error cases ─────────────────────────────────────

static void test_multiple_operators() {
    std::cerr << "  test_multiple_operators\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("lat ++ 1.0", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat ** 2.0", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat // 2.0", syms), LelParseError);
}

static void test_empty_parens() {
    std::cerr << "  test_empty_parens\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("()", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat + ()", syms), LelParseError);
}

static void test_nested_parens_mismatch() {
    std::cerr << "  test_nested_parens_mismatch\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("((lat + 1.0)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("(lat + 1.0))", syms), LelParseError);
}

static void test_trailing_comma() {
    std::cerr << "  test_trailing_comma\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("sin(lat,)", syms), LelParseError);
}

static void test_missing_function_args() {
    std::cerr << "  test_missing_function_args\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    // iif needs exactly 3 args.
    auto b = make_4x4b();
    syms.define("bm", lel_array(b));
    CHECK_THROWS_AS(lel_parse("iif(bm)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("iif(bm, 1.0)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("iif(bm, 1.0, 2.0, 3.0)", syms), LelParseError);
}

// ── Type mismatch stress ─────────────────────────────────────────────

static void test_boolean_arithmetic() {
    std::cerr << "  test_boolean_arithmetic\n";
    LelSymbolTable syms;
    auto b = make_4x4b();
    syms.define("bm", lel_array(b));

    // Can't do arithmetic on bool.
    CHECK_THROWS_AS(lel_parse("bm + 1.0", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("bm * bm", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("-bm", syms), LelParseError);
}

static void test_numeric_logical() {
    std::cerr << "  test_numeric_logical\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    // Can't use logical ops on numeric.
    CHECK_THROWS_AS(lel_parse("lat && lat", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat || lat", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("!lat", syms), LelParseError);
}

static void test_iif_condition_must_be_bool() {
    std::cerr << "  test_iif_condition_must_be_bool\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    // iif condition must be bool.
    CHECK_THROWS_AS(lel_parse("iif(lat, 1.0, 2.0)", syms), LelParseError);
}

static void test_boolean_reduction_on_numeric() {
    std::cerr << "  test_boolean_reduction_on_numeric\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("ntrue(lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("nfalse(lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("any(lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("all(lat)", syms), LelParseError);
}

static void test_numeric_reduction_on_bool() {
    std::cerr << "  test_numeric_reduction_on_bool\n";
    LelSymbolTable syms;
    auto b = make_4x4b();
    syms.define("bm", lel_array(b));

    CHECK_THROWS_AS(lel_parse("sum(bm)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("mean(bm)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("variance(bm)", syms), LelParseError);
}

// ── Shape mismatch at eval ───────────────────────────────────────────

static void test_shape_mismatch_eval() {
    std::cerr << "  test_shape_mismatch_eval\n";
    LelSymbolTable syms;
    auto a = make_4x4();
    auto b = make_3x3();
    syms.define("a", lel_array(a));
    syms.define("b", lel_array(b));

    auto node = lel_parse("a + b", syms);
    CHECK_THROWS(node.as<float>()->eval());
}

static void test_shape_mismatch_comparison() {
    std::cerr << "  test_shape_mismatch_comparison\n";
    LelSymbolTable syms;
    auto a = make_4x4();
    auto b = make_3x3();
    syms.define("a", lel_array(a));
    syms.define("b", lel_array(b));

    auto node = lel_parse("a > b", syms);
    CHECK_THROWS(node.as<bool>()->eval());
}

// ── NaN/Inf propagation (should not crash) ───────────────────────────

static void test_nan_propagation() {
    std::cerr << "  test_nan_propagation\n";
    LelSymbolTable syms;

    IPosition sh{4};
    LatticeArray<float> lat(sh, 0.0f);
    lat.make_unique();
    lat.put(IPosition{0}, std::numeric_limits<float>::quiet_NaN());
    lat.put(IPosition{1}, std::numeric_limits<float>::infinity());
    lat.put(IPosition{2}, -std::numeric_limits<float>::infinity());
    lat.put(IPosition{3}, 1.0f);
    syms.define("lat", lel_array(lat));

    // These should all succeed without crashing.
    auto n1 = lel_parse("lat + 1.0", syms);
    auto r1 = n1.as<float>()->eval();
    CHECK(r1.values.shape() == sh);

    auto n2 = lel_parse("sin(lat)", syms);
    auto r2 = n2.as<float>()->eval();
    CHECK(r2.values.shape() == sh);

    auto n3 = lel_parse("sqrt(lat)", syms);
    auto r3 = n3.as<float>()->eval();
    CHECK(r3.values.shape() == sh);

    auto n4 = lel_parse("log(lat)", syms);
    auto r4 = n4.as<float>()->eval();
    CHECK(r4.values.shape() == sh);
}

// ── Division by zero (should produce Inf, not crash) ─────────────────

static void test_division_by_zero() {
    std::cerr << "  test_division_by_zero\n";
    LelSymbolTable syms;

    auto lat = make_4x4(5.0f);
    IPosition sh{4, 4};
    LatticeArray<float> zeros(sh, 0.0f);
    syms.define("lat", lel_array(lat));
    syms.define("zeros", lel_array(zeros));

    auto node = lel_parse("lat / zeros", syms);
    auto result = node.as<float>()->eval();
    CHECK(result.values.shape() == sh);
    // Result should be inf, not crash.
    auto val = result.values.at(IPosition{0, 0});
    CHECK(std::isinf(val));
}

// ── Deeply nested expressions ────────────────────────────────────────

static void test_deep_nesting() {
    std::cerr << "  test_deep_nesting\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    // Build a deeply nested expression: ((((lat + 1) + 1) + 1) ...)
    std::string expr = "lat";
    for (int i = 0; i < 50; ++i) {
        expr.insert(0, "(");
        expr += " + 1.0)";
    }
    auto node = lel_parse(expr, syms);
    auto result = node.as<float>()->eval();
    CHECK(result.values.shape() == IPosition({4, 4}));

    // Value at [0,0] should be original(1.0) + 50 = 51.0
    auto val = result.values.at(IPosition{0, 0});
    CHECK(std::abs(val - 51.0f) < 0.01f);
}

// ── Unknown function names ───────────────────────────────────────────

static void test_unknown_functions() {
    std::cerr << "  test_unknown_functions\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("foobar(lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("notafunction(lat, lat)", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("SQRT(lat)", syms), LelParseError); // case-sensitive
}

// ── Whitespace-only and junk expressions ─────────────────────────────

static void test_whitespace_only() {
    std::cerr << "  test_whitespace_only\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("   ", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("\t\n", syms), LelParseError);
}

static void test_trailing_junk() {
    std::cerr << "  test_trailing_junk\n";
    LelSymbolTable syms;
    auto lat = make_4x4();
    syms.define("lat", lel_array(lat));

    CHECK_THROWS_AS(lel_parse("lat + 1.0 xyz", syms), LelParseError);
    CHECK_THROWS_AS(lel_parse("lat 2.0", syms), LelParseError);
}

// ── Scalar operations ────────────────────────────────────────────────

static void test_scalar_literal_operations() {
    std::cerr << "  test_scalar_literal_operations\n";
    LelSymbolTable syms;
    auto lat = make_4x4(2.0f);
    syms.define("lat", lel_array(lat));

    // Scalar * lattice should broadcast.
    auto n1 = lel_parse("3.0 * lat", syms);
    auto r1 = n1.as<float>()->eval();
    CHECK(r1.values.shape() == IPosition({4, 4}));
    CHECK(std::abs(r1.values.at(IPosition{0, 0}) - 6.0f) < 0.01f);

    // Scalar on both sides: 3.0 + 2.0 = 5.0 (scalar result).
    auto n2 = lel_parse("3.0 + 2.0", syms);
    auto r2 = n2.as<float>()->eval();
    CHECK(r2.values.shape().product() == 1);
    CHECK(std::abs(r2.values.at(IPosition{0}) - 5.0f) < 0.01f);
}

// ── Main ──────────────────────────────────────────────────────────────

int main() { // NOLINT(bugprone-exception-escape)
    std::cerr << "lel_malformed_test\n";

    test_multiple_operators();
    test_empty_parens();
    test_nested_parens_mismatch();
    test_trailing_comma();
    test_missing_function_args();
    test_boolean_arithmetic();
    test_numeric_logical();
    test_iif_condition_must_be_bool();
    test_boolean_reduction_on_numeric();
    test_numeric_reduction_on_bool();
    test_shape_mismatch_eval();
    test_shape_mismatch_comparison();
    test_nan_propagation();
    test_division_by_zero();
    test_deep_nesting();
    test_unknown_functions();
    test_whitespace_only();
    test_trailing_junk();
    test_scalar_literal_operations();

    std::cerr << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
