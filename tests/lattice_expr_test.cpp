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

#define CHECK_NEAR(a, b, tol)                                                  \
    CHECK(std::abs((a) - (b)) < (tol))

// ── Helper: create a known 4x4 float lattice ────────────────────────

static LatticeArray<float> make_4x4() {
    IPosition sh{4, 4};
    LatticeArray<float> arr(sh, 0.0f);
    arr.make_unique();
    for (std::int64_t j = 0; j < 4; ++j) {
        for (std::int64_t i = 0; i < 4; ++i) {
            arr.put(IPosition{i, j}, static_cast<float>(i + j * 4 + 1));
        }
    }
    return arr; // values 1..16
}

static LatticeArray<float> make_4x4_alt() {
    IPosition sh{4, 4};
    LatticeArray<float> arr(sh, 0.0f);
    arr.make_unique();
    for (std::int64_t j = 0; j < 4; ++j) {
        for (std::int64_t i = 0; i < 4; ++i) {
            arr.put(IPosition{i, j}, static_cast<float>((i + j * 4 + 1) * 2));
        }
    }
    return arr; // values 2,4,6,...,32
}

// ── Scalar tests ────────────────────────────────────────────────────

static void test_scalar_constant() {
    auto s = lel_scalar(42.0f);
    CHECK(s->is_scalar());
    auto r = s->eval();
    CHECK_NEAR(r.values.flat()[0], 42.0f, 1e-6f);
    CHECK(!r.mask.has_value());
}

// ── Lattice ref tests ───────────────────────────────────────────────

static void test_array_ref() {
    auto data = make_4x4();
    auto node = lel_array(data);
    CHECK(!node->is_scalar());
    CHECK(node->shape()->operator[](0) == 4);
    auto r = node->eval();
    CHECK_NEAR(r.values.flat()[0], 1.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 16.0f, 1e-6f);
}

// ── Arithmetic binary tests ─────────────────────────────────────────

static void test_add_lattice_lattice() {
    auto a = lel_array(make_4x4());
    auto b = lel_array(make_4x4_alt());
    auto expr = lel_add(a, b);
    auto r = expr->eval();
    // 1+2=3, 2+4=6, ..., 16+32=48
    CHECK_NEAR(r.values.flat()[0], 3.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 48.0f, 1e-6f);
}

static void test_sub_lattice_scalar() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(1.0f);
    auto expr = lel_sub(a, s);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 0.0f, 1e-6f);  // 1-1=0
    CHECK_NEAR(r.values.flat()[15], 15.0f, 1e-6f); // 16-1=15
}

static void test_mul_scalar_lattice() {
    auto s = lel_scalar(2.0f);
    auto a = lel_array(make_4x4());
    auto expr = lel_mul(s, a);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 2.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 32.0f, 1e-6f);
}

static void test_div_lattice_lattice() {
    auto a = lel_array(make_4x4_alt());
    auto b = lel_array(make_4x4());
    auto expr = lel_div(a, b);
    auto r = expr->eval();
    // All should be 2.0.
    CHECK_NEAR(r.values.flat()[0], 2.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 2.0f, 1e-6f);
}

static void test_div_by_zero() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(0.0f);
    auto expr = lel_div(a, s);
    auto r = expr->eval();
    // Division by zero → inf.
    CHECK(std::isinf(r.values.flat()[0]));
}

// ── Unary tests ─────────────────────────────────────────────────────

static void test_negate() {
    auto a = lel_array(make_4x4());
    auto expr = lel_negate(a);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], -1.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], -16.0f, 1e-6f);
}

// ── Comparison tests ────────────────────────────────────────────────

static void test_compare_gt() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(8.0f);
    auto expr = lel_compare(LelBinaryOp::gt, a, s);
    auto r = expr->eval();
    // Values > 8: 9..16 → last 8 elements true.
    CHECK(r.values.flat()[0] == false);  // 1 > 8 = false
    CHECK(r.values.flat()[8] == true);   // 9 > 8 = true
    CHECK(r.values.flat()[15] == true);  // 16 > 8 = true
}

static void test_compare_eq() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(5.0f);
    auto expr = lel_compare(LelBinaryOp::eq, a, s);
    auto r = expr->eval();
    CHECK(r.values.flat()[4] == true);  // 5 == 5
    CHECK(r.values.flat()[0] == false); // 1 == 5
}

// ── Logical tests ───────────────────────────────────────────────────

static void test_logical_and_or_not() {
    auto a = lel_array(make_4x4());
    auto gt5 = lel_compare(LelBinaryOp::gt, a, lel_scalar(5.0f));
    auto lt12 = lel_compare(LelBinaryOp::lt, a, lel_scalar(12.0f));
    auto and_expr = lel_and(gt5, lt12);
    auto r = and_expr->eval();
    // true for 6..11 (indices 5..10)
    CHECK(r.values.flat()[5] == true);   // 6 > 5 && 6 < 12
    CHECK(r.values.flat()[10] == true);  // 11 > 5 && 11 < 12
    CHECK(r.values.flat()[11] == false); // 12 > 5 && 12 < 12 → false
    CHECK(r.values.flat()[0] == false);  // 1 > 5 → false

    auto not_expr = lel_not(gt5);
    auto rn = not_expr->eval();
    CHECK(rn.values.flat()[0] == true);  // !(1 > 5) = true
    CHECK(rn.values.flat()[5] == false); // !(6 > 5) = false
}

// ── Math function tests ─────────────────────────────────────────────

static void test_math_sqrt() {
    auto a = lel_array(make_4x4());
    auto expr = lel_math1(LelFunc::sqrt_f, a);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 1.0f, 1e-6f);   // sqrt(1)
    CHECK_NEAR(r.values.flat()[3], 2.0f, 1e-6f);    // sqrt(4)
    CHECK_NEAR(r.values.flat()[8], 3.0f, 1e-6f);    // sqrt(9)
    CHECK_NEAR(r.values.flat()[15], 4.0f, 1e-6f);   // sqrt(16)
}

static void test_math_abs() {
    auto a = lel_array(make_4x4());
    auto neg = lel_negate(a);
    auto expr = lel_math1(LelFunc::abs_f, neg);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 1.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 16.0f, 1e-6f);
}

static void test_math_pow() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(2.0f);
    auto expr = lel_math2(LelFunc::pow_f, a, s);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 1.0f, 1e-6f);    // 1^2
    CHECK_NEAR(r.values.flat()[2], 9.0f, 1e-6f);    // 3^2
    CHECK_NEAR(r.values.flat()[15], 256.0f, 1e-6f); // 16^2
}

static void test_math_min2_max2() {
    auto a = lel_array(make_4x4());
    auto b = lel_array(make_4x4_alt());
    auto mn = lel_math2(LelFunc::min2_f, a, b);
    auto mx = lel_math2(LelFunc::max2_f, a, b);
    auto rmn = mn->eval();
    auto rmx = mx->eval();
    CHECK_NEAR(rmn.values.flat()[0], 1.0f, 1e-6f);  // min(1,2)=1
    CHECK_NEAR(rmx.values.flat()[0], 2.0f, 1e-6f);  // max(1,2)=2
}

// ── Reduction tests ─────────────────────────────────────────────────

static void test_reduce_sum() {
    auto a = lel_array(make_4x4());
    auto expr = lel_reduce(LelFunc::sum_r, a);
    CHECK(expr->is_scalar());
    auto r = expr->eval();
    // sum(1..16) = 136
    CHECK_NEAR(r.values.flat()[0], 136.0f, 1e-6f);
}

static void test_reduce_min_max() {
    auto a = lel_array(make_4x4());
    auto mn = lel_reduce(LelFunc::min_r, a);
    auto mx = lel_reduce(LelFunc::max_r, a);
    CHECK_NEAR(mn->eval().values.flat()[0], 1.0f, 1e-6f);
    CHECK_NEAR(mx->eval().values.flat()[0], 16.0f, 1e-6f);
}

static void test_reduce_mean() {
    auto a = lel_array(make_4x4());
    auto expr = lel_reduce(LelFunc::mean_r, a);
    auto r = expr->eval();
    // mean(1..16) = 8.5
    CHECK_NEAR(r.values.flat()[0], 8.5f, 1e-4f);
}

static void test_reduce_median() {
    auto a = lel_array(make_4x4());
    auto expr = lel_reduce(LelFunc::median_r, a);
    auto r = expr->eval();
    // median of 1..16 (even count) = (8+9)/2 = 8.5
    CHECK_NEAR(r.values.flat()[0], 8.5f, 1e-4f);
}

static void test_reduce_stddev() {
    auto a = lel_array(make_4x4());
    auto expr = lel_reduce(LelFunc::stddev_r, a);
    auto r = expr->eval();
    // stddev of 1..16 (sample) ≈ 4.7609
    CHECK_NEAR(r.values.flat()[0], 4.7609f, 0.01f);
}

// ── Mask-aware reduction tests ──────────────────────────────────────

static void test_reduce_with_mask() {
    auto data = make_4x4();
    IPosition sh{4, 4};
    LatticeArray<bool> mask(sh, true);
    mask.make_unique();
    // Mask out values > 8 (indices 8..15).
    for (std::int64_t i = 8; i < 16; ++i) {
        mask.put(delinearize(i, sh), false);
    }
    auto node = lel_array(data, std::optional<LatticeArray<bool>>(mask));
    auto sum_expr = lel_reduce(LelFunc::sum_r, node);
    auto r = sum_expr->eval();
    // sum(1..8) = 36
    CHECK_NEAR(r.values.flat()[0], 36.0f, 1e-6f);
}

// ── Conditional (iif) tests ─────────────────────────────────────────

static void test_iif() {
    auto a = lel_array(make_4x4());
    auto cond = lel_compare(LelBinaryOp::gt, a, lel_scalar(8.0f));
    auto true_val = lel_scalar(1.0f);
    auto false_val = lel_scalar(0.0f);
    auto expr = lel_iif(cond, true_val, false_val);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 0.0f, 1e-6f);   // 1 > 8 → 0
    CHECK_NEAR(r.values.flat()[8], 1.0f, 1e-6f);    // 9 > 8 → 1
    CHECK_NEAR(r.values.flat()[15], 1.0f, 1e-6f);   // 16 > 8 → 1
}

// ── Mask propagation tests ──────────────────────────────────────────

static void test_mask_propagation() {
    IPosition sh{4, 4};
    LatticeArray<bool> mask1(sh, true);
    mask1.make_unique();
    mask1.put(IPosition{0, 0}, false); // mask out first element

    auto a = lel_array(make_4x4(), std::optional<LatticeArray<bool>>(mask1));
    auto b = lel_array(make_4x4_alt());
    auto expr = lel_add(a, b);
    auto r = expr->eval();
    // Mask from a should propagate.
    CHECK(r.mask.has_value());
    CHECK(r.mask->flat()[0] == false); // masked
    CHECK(r.mask->flat()[1] == true);  // unmasked
}

// ── LatticeExpr tests ───────────────────────────────────────────────

static void test_lattice_expr_wrapper() {
    auto a = lel_array(make_4x4());
    auto s = lel_scalar(2.0f);
    auto tree = lel_mul(s, a);

    LatticeExpr<float> expr(tree);
    CHECK(expr.shape().has_value());
    auto r = expr.eval();
    CHECK_NEAR(r.values.flat()[0], 2.0f, 1e-6f);
    CHECK_NEAR(r.values.flat()[15], 32.0f, 1e-6f);
}

// ── LatticeExprNode (type-erased) tests ─────────────────────────────

static void test_expr_node_type_erased() {
    auto a = lel_array(make_4x4());
    LatticeExprNode node(a);
    CHECK(node.result_type() == LelType::float32);
    CHECK(node.is_valid());
    auto typed = node.as<float>();
    auto r = typed->eval();
    CHECK_NEAR(r.values.flat()[0], 1.0f, 1e-6f);
}

// ── Shape mismatch error test ───────────────────────────────────────

static void test_shape_mismatch() {
    LatticeArray<float> a(IPosition{4, 4}, 1.0f);
    LatticeArray<float> b(IPosition{3, 3}, 2.0f);
    auto na = lel_array(a);
    auto nb = lel_array(b);
    auto expr = lel_add(na, nb);
    bool threw = false;
    try {
        auto r = expr->eval();
        (void)r;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
}

// ── Scalar-scalar operations ────────────────────────────────────────

static void test_scalar_scalar() {
    auto a = lel_scalar(3.0f);
    auto b = lel_scalar(4.0f);
    auto expr = lel_add(a, b);
    CHECK(expr->is_scalar());
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 7.0f, 1e-6f);
}

// ── Double precision tests ──────────────────────────────────────────

static void test_double_arithmetic() {
    LatticeArray<double> data(IPosition{4}, 0.0);
    data.make_unique();
    data.put(IPosition{0}, 1.0);
    data.put(IPosition{1}, 2.0);
    data.put(IPosition{2}, 3.0);
    data.put(IPosition{3}, 4.0);

    auto a = lel_array(data);
    auto s = lel_scalar(0.5);
    auto expr = lel_mul(a, s);
    auto r = expr->eval();
    CHECK_NEAR(r.values.flat()[0], 0.5, 1e-10);
    CHECK_NEAR(r.values.flat()[3], 2.0, 1e-10);
}

// ── Main ─────────────────────────────────────────────────────────────

int main() {
    test_scalar_constant();
    test_array_ref();

    // Arithmetic
    test_add_lattice_lattice();
    test_sub_lattice_scalar();
    test_mul_scalar_lattice();
    test_div_lattice_lattice();
    test_div_by_zero();

    // Unary
    test_negate();

    // Comparison
    test_compare_gt();
    test_compare_eq();

    // Logical
    test_logical_and_or_not();

    // Math functions
    test_math_sqrt();
    test_math_abs();
    test_math_pow();
    test_math_min2_max2();

    // Reductions
    test_reduce_sum();
    test_reduce_min_max();
    test_reduce_mean();
    test_reduce_median();
    test_reduce_stddev();
    test_reduce_with_mask();

    // Conditional
    test_iif();

    // Mask propagation
    test_mask_propagation();

    // Wrappers
    test_lattice_expr_wrapper();
    test_expr_node_type_erased();

    // Error cases
    test_shape_mismatch();

    // Scalar-scalar
    test_scalar_scalar();

    // Double precision
    test_double_arithmetic();

    std::cout << "lattice_expr_test: " << g_pass << " passed, " << g_fail
              << " failed\n";
    return g_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
